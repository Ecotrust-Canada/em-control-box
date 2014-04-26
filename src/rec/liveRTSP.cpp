#include "settings.h"
#include "output.h"
#include "liveRTSP.h"
#include <groupsock/GroupsockHelper.hh>

using namespace std;

extern UsageEnvironment *G_env;
extern char             *G_captureLoopWatchPtr;
extern MultiRTSPClient  *G_rtspClients[DIGITAL_MAX_CAMS];
extern unsigned          G_rtspClientCount;
extern string moduleName;

QuickTimeFileSink* qtOut[DIGITAL_MAX_CAMS] = { NULL, NULL, NULL, NULL };
int totNumPacketsReceived[DIGITAL_MAX_CAMS] = { ~0, ~0, ~0, ~0 };

StreamClientState::StreamClientState(): iter(NULL), session(NULL), subsession(NULL), streamTimerTask(NULL), duration(0.0) {}

StreamClientState::~StreamClientState() {
  delete iter;
}

MultiRTSPClient::MultiRTSPClient(UsageEnvironment& G_env,
                                 char const* rtspURL,
                                 int verbosityLevel,
                                 char const* applicationName,
                                 //portNumBits tunnelOverHTTPPortNum,
                                 string _videoDirectory,
                                 unsigned short _camIndex,
                                 unsigned short _clipLength,
                                 EM_DATA_TYPE *_em_data):
  RTSPClient(G_env, rtspURL, verbosityLevel, applicationName, 0, -1) {
    origRTSPUrl = rtspURL;
    videoDirectory = _videoDirectory;
    camIndex = _camIndex;
    clipLength = _clipLength;
    em_data = _em_data;
    frameRate = 0;
  }

MultiRTSPClient::~MultiRTSPClient() {}

// Implementation of the RTSP 'response handlers':
void continueAfterDESCRIBE(RTSPClient* rtspClient, int resultCode, char* resultString) {
  D("continueAfterDESCRIBE()");
  do {
    StreamClientState& scs = ((MultiRTSPClient*)rtspClient)->scs; // alias

    if (resultCode != 0) {
      E("Failed to get a SDP description: " + resultString);
      break;
    }

    char* const sdpDescription = resultString;
    D("Got a SDP description");

    // Create a media session object from this SDP description:
    scs.session = MediaSession::createNew(*G_env, sdpDescription);
    delete[] sdpDescription; // because we don't need it anymore

    if (scs.session == NULL) {
      E("Failed to create a MediaSession object from the SDP description: " + G_env->getResultMsg());
      break;
    } else if (!scs.session->hasSubsessions()) {
      E("This session has no media subsessions (i.e., no \"m=\" lines)");
      break;
    }

    // Then, create and set up our data source objects for the session.  We do this by iterating over the session's 'subsessions',
    // calling "MediaSubsession::initiate()", and then sending a RTSP "SETUP" command, on each one.
    // (Each 'subsession' will have its own data source.)
    scs.iter = new MediaSubsessionIterator(*scs.session);
    setupNextSubsession(rtspClient);
    return;
  } while (0);
  delete[] resultString;

  // An unrecoverable error occurred with this stream.
  shutdownStream(rtspClient, LIVERTSP_EXIT_EARLY_RTSP_ERROR);
}

// By default, we request that the server stream its data using RTP/UDP.
// If, instead, you want to request that the server stream via RTP-over-TCP, change the following to True:
void setupNextSubsession(RTSPClient* rtspClient) {
  D("setupNextSubsession()");
  StreamClientState& scs = ((MultiRTSPClient*)rtspClient)->scs; // alias
  
  scs.subsession = scs.iter->next();
  if (scs.subsession != NULL) {
    if (!scs.subsession->initiate()) {
      E("Failed to initiate the \"" + scs.subsession->mediumName() + "/" + scs.subsession->codecName() + "\" subsession: " + G_env->getResultMsg());
      setupNextSubsession(rtspClient); // give up on this subsession; go to the next one
    } else {
      D("Initiated the \"" + scs.subsession->mediumName() + "/" + scs.subsession->codecName() + "\" subsession (client ports " + to_string(scs.subsession->clientPortNum()) + "-" + to_string(scs.subsession->clientPortNum() + 1) + ")");

      if (scs.subsession->rtpSource() != NULL) {
        // Because we're saving the incoming data, rather than playing
        // it in real time, allow an especially large time threshold
        // (1 second) for reordering misordered incoming packets:
        unsigned const thresh = 1000000; // 1 second
        scs.subsession->rtpSource()->setPacketReorderingThresholdTime(thresh);
        
        // Set the RTP source's OS socket buffer size as appropriate - either if we were explicitly asked (using -B),
        // or if the desired FileSink buffer size happens to be larger than the current OS socket buffer size.
        // (The latter case is a heuristic, on the assumption that if the user asked for a large FileSink buffer size,
        // then the input data rate may be large enough to justify increasing the OS socket buffer size also.)
        int socketNum = scs.subsession->rtpSource()->RTPgs()->socketNum();
        //unsigned curBufferSize = getReceiveBufferSize(*G_env, socketNum);
        
        /*
        unsigned newBufferSize;
        newBufferSize = setReceiveBufferTo(*G_env, socketNum, SOCKET_FILE_BUFFER_SIZE);
        D("Changed socket receive buffer size to " + to_string(newBufferSize) + " bytes");
        */
        setReceiveBufferTo(*G_env, socketNum, SOCKET_FILE_BUFFER_SIZE);
      }
    
      // Continue setting up this subsession, by sending a RTSP "SETUP" command:
      rtspClient->sendSetupCommand(*scs.subsession, continueAfterSETUP, False, REQUEST_STREAMING_OVER_TCP);
    }

    return;
  }

  // We've finished setting up all of the subsessions. Let's deal with the output sinks
  createPeriodicOutputFiles(rtspClient);

  // Now, send a RTSP "PLAY" command to start the streaming:
  if (scs.session->absStartTime() != NULL) {
    // Special case: The stream is indexed by 'absolute' time, so send an appropriate "PLAY" command:
    rtspClient->sendPlayCommand(*scs.session, continueAfterPLAY, scs.session->absStartTime(), scs.session->absEndTime());
  } else {
    scs.duration = scs.session->playEndTime() - scs.session->playStartTime();
    rtspClient->sendPlayCommand(*scs.session, continueAfterPLAY);
  }
}

void createOutputFiles(RTSPClient *rtspClient, char const *datePrefix) {
  D("createOutputFiles()");
  StreamClientState& scs = ((MultiRTSPClient*)rtspClient)->scs; // alias
  EM_DATA_TYPE *em_data = ((MultiRTSPClient*)rtspClient)->em_data;
  unsigned short camIndex = ((MultiRTSPClient*)rtspClient)->camIndex;
  char fileName[128];

  pthread_mutex_lock(&(em_data->mtx));
    snprintf(fileName, sizeof(fileName), "%s/%s-cam%d.mp4", (em_data->SYS_targetDisk + ((MultiRTSPClient*)rtspClient)->videoDirectory).c_str(), datePrefix, camIndex + 1);
  pthread_mutex_unlock(&(em_data->mtx));

  qtOut[camIndex] = QuickTimeFileSink::createNew(*G_env, *scs.session,
    //(string(fileName) + ".current").c_str(),
    fileName,
    SOCKET_FILE_BUFFER_SIZE,
    DIGITAL_OUTPUT_WIDTH,
    DIGITAL_OUTPUT_HEIGHT,
    ((MultiRTSPClient*)rtspClient)->frameRate, /*movieframeRate*/
    false, /*packetLossCompensate*/
    false, /*syncStreams*/
    false, /*generateHintTracks*/
    true /*generateMP4Format*/);

    if (qtOut[camIndex] == NULL) {
      E("Failed to create a \"QuickTimeFileSink\" for outputting to \"" + fileName + "\": " + G_env->getResultMsg());
      shutdownStream(rtspClient, LIVERTSP_EXIT_OUTPUT_FILE_PROBLEM);
    }

    qtOut[camIndex]->startPlaying(closeSinkAfterPlaying, NULL);
    __SYS_SET_STATE(SYS_VIDEO_RECORDING);
    I("Recording video to " + fileName);
}

void createPeriodicOutputFiles(RTSPClient* rtspClient) {
  D("createPeriodicOutputFiles()");
  StreamClientState& scs = ((MultiRTSPClient*)rtspClient)->scs; // alias
  EM_DATA_TYPE *em_data = ((MultiRTSPClient*)rtspClient)->em_data;
  unsigned short frameRate;
  unsigned nextClipDelay = 1;

  if(__SYS_GET_STATE & SYS_TARGET_DISK_WRITABLE) {
    if(__SYS_GET_STATE & SYS_REDUCED_VIDEO_BITRATE) {
      frameRate = DIGITAL_OUTPUT_FPS_SLOW;
    } else {
      frameRate = DIGITAL_OUTPUT_FPS_NORMAL;
    }
    // change FPS if needed
    if(((MultiRTSPClient*)rtspClient)->frameRate != frameRate) {
        setOutputFrameRate(rtspClient, frameRate);
        ((MultiRTSPClient*)rtspClient)->frameRate = frameRate;
    }

    time_t rawtime;
    struct tm *timeinfo;
    char date[32];

    time(&rawtime);
    timeinfo = localtime(&rawtime);
    strftime(date, sizeof(date), "%Y%m%d-%H%M%S", timeinfo);

    createOutputFiles(rtspClient, date);

    nextClipDelay = MAX_CLIP_LENGTH - (time(0) % MAX_CLIP_LENGTH);
    I("Next video in " + to_string(nextClipDelay) + "s (@ " + date +")");
  } else {
    E("No writable disks; won't schedule next clip");
    shutdownStream(rtspClient, LIVERTSP_EXIT_OUTPUT_FILE_PROBLEM);
    return;
  }

  // Schedule an event for writing the next output file:
  scs.periodicFileOutputTask = G_env->taskScheduler().scheduleDelayedTask(nextClipDelay * 1000000, (TaskFunc*)periodicFileOutputTimerHandler, rtspClient);
}

void periodicFileOutputTimerHandler(void* clientData) {
  D("periodicFileOutputTimerHandler()");
  
  // First, close the existing output files:
  closeMediaSinks((RTSPClient*)clientData);

  // Then, create new output files:
  createPeriodicOutputFiles((RTSPClient*)clientData);
}

void closeMediaSinks(RTSPClient* rtspClient) {
  D("closeMediaSinks()");
  unsigned short camIndex = ((MultiRTSPClient*)rtspClient)->camIndex;
  EM_DATA_TYPE *em_data = ((MultiRTSPClient*)rtspClient)->em_data;

  if(qtOut[camIndex] != NULL) {
      Medium::close(qtOut[camIndex]);
      qtOut[camIndex] = NULL;
      D("Closed QT sink for cam " + to_string(camIndex + 1));
  }
  
  __SYS_UNSET_STATE(SYS_VIDEO_RECORDING);
}

void setOutputFrameRate(RTSPClient* rtspClient, unsigned short frameRate) {
    D("setOutputFrameRate()");
    unsigned short camIndex = ((MultiRTSPClient*)rtspClient)->camIndex;
    char url[128], command[160];
    string api_call = DIGITAL_HTTP_API_FPS;
    api_call = api_call + to_string(frameRate);

    snprintf(url, sizeof(url), DIGITAL_HTTP_API_URL, camIndex + 1, api_call.c_str());
    snprintf(command, sizeof(command), DIGITAL_HTTP_API_COMMAND, url);
    D("Executing: " + command);
    system(command);
}

void continueAfterSETUP(RTSPClient* rtspClient, int resultCode, char* resultString) {
  D("continueAfterSETUP()");
  do {
    StreamClientState& scs = ((MultiRTSPClient*)rtspClient)->scs; // alias

    if (resultCode != 0) {
      E("Failed to set up the \"" + scs.subsession->mediumName() + "/" + scs.subsession->codecName() + "\" subsession: " + resultString);
      break;
    }

    D("Set up the \"" + scs.subsession->mediumName() + "/" + scs.subsession->codecName() + "\" subsession (client ports " + to_string(scs.subsession->clientPortNum()) + "-" + to_string(scs.subsession->clientPortNum() + 1) + ")");

    scs.subsession->miscPtr = rtspClient; // a hack to let subsession handle functions get the "RTSPClient" from the subsession 
    //scs.subsession->sink->startPlaying(*(scs.subsession->readSource()), closeSubsession, scs.subsession);

    // Also set a handler to be called if a RTCP "BYE" arrives for this subsession:
    if (scs.subsession->rtcpInstance() != NULL) {
      scs.subsession->rtcpInstance()->setByeHandler(subsessionByeHandler, scs.subsession);
    }
//////////////////////////////////////
  } while (0);
  delete[] resultString;

  // Set up the next subsession, if any:
  setupNextSubsession(rtspClient);
}

void continueAfterPLAY(RTSPClient* rtspClient, int resultCode, char* resultString) {
  D("continueAfterPLAY() (I pretty much do nothing)");
  Boolean success = False;

  do {
    //StreamClientState& scs = ((MultiRTSPClient*)rtspClient)->scs; // alias

    if (resultCode != 0) {
      E("Failed to start playing session: " + resultString);
      break;
    }

    // Set a timer to be handled at the end of the stream's expected duration (if the stream does not already signal its end
    // using a RTCP "BYE").  This is optional.  If, instead, you want to keep the stream active - e.g., so you can later
    // 'seek' back within it and do another RTSP "PLAY" - then you can omit this code.
    // (Alternatively, if you don't want to receive the entire stream, you could set this timer for some shorter value.)
    /*if (scs.duration > 0) {
      unsigned uSecsToDelay = (unsigned)(scs.duration*1000000);
      scs.streamTimerTask = G_env.taskScheduler().scheduleDelayedTask(uSecsToDelay, (TaskFunc*)streamTimerHandler, (void*)rtspClient);
    }*/
    
    /*if (scs.duration > 0) {
      *G_env << " (for up to " << scs.duration << " seconds)";
    }*/

    success = True;

    // Watch for incoming packets (if desired):
    checkInterPacketGaps(rtspClient);
  } while (0);
  delete[] resultString;

  if (!success) {
    // An unrecoverable error occurred with this stream.
    shutdownStream(rtspClient, LIVERTSP_EXIT_GENERAL_RTSP_ERROR);
  }
}

void closeSubsessionAfterPlaying(void* clientData) {
  D("closeSubsessionAfterPlaying()");
  MediaSubsession* subsession = (MediaSubsession*)clientData;
  RTSPClient* rtspClient = (RTSPClient*)(subsession->miscPtr);

  // Begin by closing this subsession's stream:
  Medium::close(subsession->sink);
  subsession->sink = NULL;

  // Next, check whether *all* subsessions' streams have now been closed:
  MediaSession& session = subsession->parentSession();
  MediaSubsessionIterator iter(session);
  while ((subsession = iter.next()) != NULL) {
    if (subsession->sink != NULL) return; // this subsession is still active
  }

  // All subsessions' streams have now been closed, so shutdown the client:
  shutdownStream(rtspClient, LIVERTSP_EXIT_CLEAN);
}

void closeSinkAfterPlaying(void* clientData) {
  D("closeSinkAfterPlaying()");
  //MediaSubsession* subsession = (MediaSubsession*)clientData;
  //RTSPClient* rtspClient = (RTSPClient*)subsession->miscPtr;

  closeMediaSinks((RTSPClient*)clientData);
}

void subsessionByeHandler(void* clientData) {
  MediaSubsession* subsession = (MediaSubsession*)clientData;
  //RTSPClient* rtspClient = (RTSPClient*)subsession->miscPtr;

  D("subsessionByeHandler() - Received RTCP \"BYE\" on \"" + subsession->mediumName() + "/" + subsession->codecName() + "\" subsession");

  // Now act as if the subsession had closed:
  closeSubsessionAfterPlaying(subsession);
}

// NOT USED
void streamTimerHandler(void* clientData) {
  D("streamTimerHandler()");
  MultiRTSPClient* rtspClient = (MultiRTSPClient*)clientData;
  StreamClientState& scs = rtspClient->scs; // alias

  scs.streamTimerTask = NULL;

  // Shut down the stream:
  shutdownStream(rtspClient, LIVERTSP_EXIT_DURATION_OVER);
}
///////////

Boolean areAlreadyShuttingDown = false;
// Used to shut down and close a stream (including its "RTSPClient" object):
void shutdownStream(RTSPClient* rtspClient, int exitCode) {
  //if (areAlreadyShuttingDown) return; // in case we're called after receiving a RTCP "BYE" while in the middle of a "TEARDOWN".
  areAlreadyShuttingDown = true;

  StreamClientState& scs = ((MultiRTSPClient*)rtspClient)->scs; // alias

  if(exitCode == LIVERTSP_EXIT_OUTPUT_FILE_PROBLEM) { E("shutdownStream() OUTPUT_FILE_PROBLEM"); }
  else if(exitCode == LIVERTSP_EXIT_SERVER_NOT_RESPONDING) { E("shutdownStream() SERVER_NOT_RESPONDING"); }
  else if(exitCode == LIVERTSP_EXIT_GENERAL_RTSP_ERROR) { E("shutdownStream() GENERAL_RTSP_ERROR"); }
  else if(exitCode == LIVERTSP_EXIT_EARLY_RTSP_ERROR) { E("shutdownStream() EARLY_RTSP_ERROR"); }
  else if(exitCode == LIVERTSP_EXIT_CLEAN) { I("shutdownStream() CLEAN"); }
  else if(exitCode == LIVERTSP_EXIT_DURATION_OVER) { I("shutdownStream() DURATION_OVER"); }
  
  if (G_env != NULL) {
    D("Unscheduling delayed tasks ...");
    if(scs.interPacketGapCheckTimerTask != NULL) G_env->taskScheduler().unscheduleDelayedTask(scs.interPacketGapCheckTimerTask);
    if(scs.periodicFileOutputTask != NULL) G_env->taskScheduler().unscheduleDelayedTask(scs.periodicFileOutputTask);
    if(scs.streamTimerTask != NULL) G_env->taskScheduler().unscheduleDelayedTask(scs.streamTimerTask);
  }

  Boolean shutdownImmediately = false;
  if(exitCode < LIVERTSP_EXIT_CLEAN) shutdownImmediately = true;

  if (scs.session != NULL) {
    RTSPClient::responseHandler* responseHandlerForTEARDOWN = NULL; // unless:
    if (!shutdownImmediately) {
      responseHandlerForTEARDOWN = continueAfterTEARDOWN;
      rtspClient->sendTeardownCommand(*scs.session, responseHandlerForTEARDOWN);
    }
  }

  if(shutdownImmediately) {
    continueAfterTEARDOWN(rtspClient, exitCode, NULL);
  }
}

void continueAfterTEARDOWN(RTSPClient* rtspClient, int exitCode, char* resultString) {
  D("continueAfterTEARDOWN()");
  StreamClientState& scs = ((MultiRTSPClient*)rtspClient)->scs; // alias
  EM_DATA_TYPE *em_data = ((MultiRTSPClient*)rtspClient)->em_data;

  closeMediaSinks(rtspClient);

  // First, check whether any subsessions have still to be closed:
  if (scs.session != NULL) { 
    Boolean someSubsessionsWereActive = False;
    MediaSubsessionIterator iter(*scs.session);
    MediaSubsession* subsession;

    while ((subsession = iter.next()) != NULL) {
      if (subsession->sink != NULL) {
        Medium::close(subsession->sink);
        subsession->sink = NULL;

        if (subsession->rtcpInstance() != NULL) {
          subsession->rtcpInstance()->setByeHandler(NULL, NULL); // in case the server sends a RTCP "BYE" while handling "TEARDOWN"
        }

        someSubsessionsWereActive = True;
      }
    }

    if (someSubsessionsWereActive) {
      D("continueAfterTEARDOWN() - Some subsessions were still active, sending TEARDOWN");
      // Send a RTSP "TEARDOWN" command, to tell the server to shutdown the stream.
      // Don't bother handling the response to the "TEARDOWN".
      rtspClient->sendTeardownCommand(*scs.session, NULL);
    }
  }

  if(exitCode >= LIVERTSP_EXIT_CLEAN) {
      D("Closing scs.session ...");
      Medium::close(scs.session);

      D("Closing rtspClient ...");
      Medium::close(rtspClient);
        // Note that this will also cause this stream's "StreamClientState" structure to get reclaimed.

      D("G_rtspClientCount is " + to_string(G_rtspClientCount));
      if (--G_rtspClientCount == 0) {
        // The final stream has ended, so exit the application now.
        // (Of course, if you're embedding this code into your own application, you might want to comment this out,
        // and replace it with "eventLoopWatchVariable = 1;", so that we leave the LIVE555 event loop, and continue running "main()".)
        //exit(exitCode);
        *G_captureLoopWatchPtr = LOOP_WATCH_VAR_NOT_RUNNING;
      }
      D("G_rtspClientCount is now " + to_string(G_rtspClientCount));
  } else {
      D("Unclean shutdown, assuming we want to keep capturing from this cam");
      usleep(100000); // wait 100 ms
      //((MultiRTSPClient*)rtspClient)->setBaseURL( ((MultiRTSPClient*)rtspClient)->origRTSPUrl.c_str() );
      
      D("Creating new MultiRTSPClient");
      MultiRTSPClient* newRTSPClient = new MultiRTSPClient(
        *G_env,
        ((MultiRTSPClient*)rtspClient)->origRTSPUrl.c_str(),
        LIVERTSP_CLIENT_VERBOSITY_LEVEL,
        "em-rec",
        ((MultiRTSPClient*)rtspClient)->videoDirectory,
        ((MultiRTSPClient*)rtspClient)->camIndex,
        MAX_CLIP_LENGTH,
        em_data);

      D("Switcheroo, and close old rtspClient ...");
      G_rtspClients[ ((MultiRTSPClient*)rtspClient)->camIndex ] = newRTSPClient;

      Medium::close(rtspClient);

      D("Beginning again ...");      
      newRTSPClient->sendDescribeCommand(continueAfterDESCRIBE);
  }

  delete[] resultString;
}

void checkInterPacketGaps(void* clientData) {
  if (LIVERTSP_INTERPACKET_GAP_TIME == 0) return; // we're not checking

  StreamClientState& scs = ((MultiRTSPClient*)clientData)->scs; // alias
  unsigned short camIndex = ((MultiRTSPClient*)clientData)->camIndex;

  // Check each subsession, counting up how many packets have been received:
  int newTotNumPacketsReceived = 0;

  MediaSubsessionIterator iter(*scs.session);
  MediaSubsession* subsession;
  while ((subsession = iter.next()) != NULL) {
    RTPSource* src = subsession->rtpSource();
    if (src == NULL) continue;
    newTotNumPacketsReceived += src->receptionStatsDB().totNumPacketsReceived();
  }

  if (newTotNumPacketsReceived == totNumPacketsReceived[camIndex]) {
    // No additional packets have been received since the last time we
    // checked, so end this stream:
    E("Stopped getting packets from camera! Closing stream ...");
    scs.interPacketGapCheckTimerTask = NULL;
    shutdownStream((RTSPClient*)clientData, LIVERTSP_EXIT_SERVER_NOT_RESPONDING);
  } else {
    totNumPacketsReceived[camIndex] = newTotNumPacketsReceived;
    // Check again, after the specified delay:
    scs.interPacketGapCheckTimerTask = G_env->taskScheduler().scheduleDelayedTask(LIVERTSP_INTERPACKET_GAP_TIME * 1000000, (TaskFunc*)checkInterPacketGaps, clientData);
  }
}