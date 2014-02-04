#include "liveRTSP.h"
#include "output.h"
#include <groupsock/GroupsockHelper.hh>

using namespace std;

extern UsageEnvironment *env;
extern char* captureLoopWatchPtr;
extern unsigned rtspClientCount;
extern string moduleName;

QuickTimeFileSink* qtOut = NULL;
string qtOutFilename;

/*
// A function that outputs a string that identifies each stream (for debugging output).  Modify this if you wish:
UsageEnvironment& operator<<(UsageEnvironment& env, const RTSPClient& rtspClient) {
  return env << "[URL:\"" << rtspClient.url() << "\"]: ";
}

// A function that outputs a string that identifies each subsession (for debugging output).  Modify this if you wish:
UsageEnvironment& operator<<(UsageEnvironment& env, const MediaSubsession& subsession) {
  return env << subsession.mediumName() << "/" << subsession.codecName();
}
*/

// subsession.mediumName() + "/" + subsession.codecName()

// rtspClient.url()
// subsession.mediumName()
// subsession.codecName();

StreamClientState::StreamClientState(): iter(NULL), session(NULL), subsession(NULL), streamTimerTask(NULL), duration(0.0) {}

StreamClientState::~StreamClientState() {
  delete iter;
  if (session != NULL) {
    // We also need to delete "session", and unschedule "streamTimerTask" (if set)
    UsageEnvironment& env = session->envir(); // alias

    env.taskScheduler().unscheduleDelayedTask(interPacketGapCheckTimerTask);
    env.taskScheduler().unscheduleDelayedTask(periodicFileOutputTask);
    env.taskScheduler().unscheduleDelayedTask(streamTimerTask);
    Medium::close(session);
  }
}

MultiRTSPClient::MultiRTSPClient(UsageEnvironment& env,
                                 char const* rtspURL,
                                 int verbosityLevel,
                                 char const* applicationName,
                                 //portNumBits tunnelOverHTTPPortNum,
                                 string _baseVideoDirectory,
                                 unsigned short _camIndex,
                                 unsigned short _clipLength):
  RTSPClient(env, rtspURL, verbosityLevel, applicationName, 0, -1) {
    baseVideoDirectory = _baseVideoDirectory;
    camIndex = _camIndex;
    clipLength = _clipLength;
  }

MultiRTSPClient::~MultiRTSPClient() {}

// Implementation of the RTSP 'response handlers':
void continueAfterDESCRIBE(RTSPClient* rtspClient, int resultCode, char* resultString) {
  do {
    StreamClientState& scs = ((MultiRTSPClient*)rtspClient)->scs; // alias

    if (resultCode != 0) {
      E("Failed to get a SDP description: " + resultString);
      break;
    }

    char* const sdpDescription = resultString;
    D("Got a SDP description:\n" + sdpDescription);

    // Create a media session object from this SDP description:
    scs.session = MediaSession::createNew(*env, sdpDescription);
    delete[] sdpDescription; // because we don't need it anymore
    if (scs.session == NULL) {
      E("Failed to create a MediaSession object from the SDP description: " + env->getResultMsg());
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
  StreamClientState& scs = ((MultiRTSPClient*)rtspClient)->scs; // alias
  
  scs.subsession = scs.iter->next();
  if (scs.subsession != NULL) {
    if (!scs.subsession->initiate()) {
      E("Failed to initiate the \"" + scs.subsession->mediumName() + "/" + scs.subsession->codecName() + "\" subsession: " + env->getResultMsg());
      setupNextSubsession(rtspClient); // give up on this subsession; go to the next one
    } else {
      D("Initiated the \"" + scs.subsession->mediumName() + "/" + scs.subsession->codecName() + "\" subsession (client ports " + to_string(scs.subsession->clientPortNum()) + "-" + to_string(scs.subsession->clientPortNum() + 1));

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
        //unsigned curBufferSize = getReceiveBufferSize(*env, socketNum);
        
        unsigned newBufferSize;
        newBufferSize = setReceiveBufferTo(*env, socketNum, SOCKET_FILE_BUFFER_SIZE);
        D("Changed socket receive buffer size to " + to_string(newBufferSize) + " bytes");
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
  StreamClientState& scs = ((MultiRTSPClient*)rtspClient)->scs; // alias
  char fileName[128];

  snprintf(fileName, sizeof(fileName), "%s/%s-cam%d.mp4", ((MultiRTSPClient*)rtspClient)->baseVideoDirectory.c_str(), datePrefix, ((MultiRTSPClient*)rtspClient)->camIndex + 1);

  qtOut = QuickTimeFileSink::createNew(*env, *scs.session,
    (string(fileName) + ".current").c_str(),
    SOCKET_FILE_BUFFER_SIZE,
    1280,/*movieWidth*/
    720,/*movieHeight*/
    1,/*movieFPS*/
    false, /*packetLossCompensate*/
    false, /*syncStreams*/
    false, /*generateHintTracks*/
    false /*generateMP4Format*/);

    if (qtOut == NULL) {
      E("Failed to create a \"QuickTimeFileSink\" for outputting to \"" + fileName + "\": " + env->getResultMsg());
      shutdownStream(rtspClient, LIVERTSP_EXIT_OUTPUT_FILE_PROBLEM);
    } else {
      D("Outputting to the file: \"" + fileName);
      qtOutFilename = fileName;
    }

    qtOut->startPlaying(closeSinkAfterPlaying, NULL);
    //scs.subsession->sink = (MediaSink*)qtOut;
}

void createPeriodicOutputFiles(RTSPClient* rtspClient) {
  StreamClientState& scs = ((MultiRTSPClient*)rtspClient)->scs; // alias

  time_t rawtime;
  struct tm *timeinfo;
  char date[32];

  time(&rawtime);
  timeinfo = localtime(&rawtime);
  strftime(date, sizeof(date), "%Y%m%d-%H%M%S", timeinfo);

  createOutputFiles(rtspClient, date);

  // Schedule an event for writing the next output file:
  scs.periodicFileOutputTask = env->taskScheduler().scheduleDelayedTask(((MultiRTSPClient*)rtspClient)->clipLength * 1000000, (TaskFunc*)periodicFileOutputTimerHandler, rtspClient);
}

void periodicFileOutputTimerHandler(void* clientData) {
  // First, close the existing output files:
  closeMediaSinks();

  // Then, create new output files:
  createPeriodicOutputFiles((RTSPClient*)clientData);
}

void closeMediaSinks() {
  Medium::close(qtOut);
  qtOut = NULL;
  D("Closed QT sink");

  rename((qtOutFilename + ".current").c_str(), qtOutFilename.c_str());

  // 

  // TODO
  // rename(fileName, (string(fileName) + ".h264").c_str()); 
  // ALSO check
  //if(errno == ENOSPC) {
  //    ((StateMachine *)em_data->sm_system)->SetState(SYS_OS_DISK_FULL);
  //}
  //
  // Check for the above, shut down properly on CTRL+C, multiple output files (one per cam)
}

void continueAfterSETUP(RTSPClient* rtspClient, int resultCode, char* resultString) {
  do {
    StreamClientState& scs = ((MultiRTSPClient*)rtspClient)->scs; // alias

    if (resultCode != 0) {
      E("Failed to set up the \"" + scs.subsession->mediumName() + "/" + scs.subsession->codecName() + "\" subsession: " + resultString);
      break;
    }

    D("Set up the \"" + scs.subsession->mediumName() + "/" + scs.subsession->codecName() + "\" subsession (client ports " + to_string(scs.subsession->clientPortNum()) + "-" + to_string(scs.subsession->clientPortNum() + 1));

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
      scs.streamTimerTask = env.taskScheduler().scheduleDelayedTask(uSecsToDelay, (TaskFunc*)streamTimerHandler, (void*)rtspClient);
    }*/

    D("Started playing session ...");
    /*if (scs.duration > 0) {
      *env << " (for up to " << scs.duration << " seconds)";
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
  //MediaSubsession* subsession = (MediaSubsession*)clientData;
  //RTSPClient* rtspClient = (RTSPClient*)subsession->miscPtr;

  D("In closeSinkAfterPlaying()");
  closeMediaSinks();
}

void subsessionByeHandler(void* clientData) {
  MediaSubsession* subsession = (MediaSubsession*)clientData;
  //RTSPClient* rtspClient = (RTSPClient*)subsession->miscPtr;

  D("Received RTCP \"BYE\" on \"" + subsession->mediumName() + "/" + subsession->codecName() + "\" subsession");

  // Now act as if the subsession had closed:
  closeSubsessionAfterPlaying(subsession);
}

void streamTimerHandler(void* clientData) {
  MultiRTSPClient* rtspClient = (MultiRTSPClient*)clientData;
  StreamClientState& scs = rtspClient->scs; // alias

  scs.streamTimerTask = NULL;

  // Shut down the stream:
  shutdownStream(rtspClient, LIVERTSP_EXIT_DURATION_OVER);
}

Boolean areAlreadyShuttingDown = false;
// Used to shut down and close a stream (including its "RTSPClient" object):
void shutdownStream(RTSPClient* rtspClient, int exitCode) {
  if (areAlreadyShuttingDown) return; // in case we're called after receiving a RTCP "BYE" while in the middle of a "TEARDOWN".
  areAlreadyShuttingDown = true;

  StreamClientState& scs = ((MultiRTSPClient*)rtspClient)->scs; // alias
  Boolean shutdownImmediately = false;

  if(exitCode == LIVERTSP_EXIT_SERVER_NOT_RESPONDING) {
    shutdownImmediately = true;
  }

  if (scs.session != NULL) {
    RTSPClient::responseHandler* responseHandlerForTEARDOWN = NULL; // unless:
    if (!shutdownImmediately) {
      responseHandlerForTEARDOWN = continueAfterTEARDOWN;
      rtspClient->sendTeardownCommand(*scs.session, responseHandlerForTEARDOWN);
    }
  }

  if(shutdownImmediately) continueAfterTEARDOWN(NULL, 0, NULL);
}

void continueAfterTEARDOWN(RTSPClient* rtspClient, int resultCode, char* resultString) {
  closeMediaSinks();

  StreamClientState& scs = ((MultiRTSPClient*)rtspClient)->scs; // alias

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
      // Send a RTSP "TEARDOWN" command, to tell the server to shutdown the stream.
      // Don't bother handling the response to the "TEARDOWN".
      rtspClient->sendTeardownCommand(*scs.session, NULL);
    }
  }

  Medium::close(scs.session);

  D("Closing the stream ...");
  Medium::close(rtspClient);
    // Note that this will also cause this stream's "StreamClientState" structure to get reclaimed.

  if (--rtspClientCount == 0) {
    // The final stream has ended, so exit the application now.
    // (Of course, if you're embedding this code into your own application, you might want to comment this out,
    // and replace it with "eventLoopWatchVariable = 1;", so that we leave the LIVE555 event loop, and continue running "main()".)
    //exit(exitCode);
    *captureLoopWatchPtr = 1;
  }

  delete[] resultString;
}

unsigned totNumPacketsReceived = ~0;
void checkInterPacketGaps(void* clientData) {
  if (LIVERTSP_INTERPACKET_GAP_TIME == 0) return; // we're not checking

  StreamClientState& scs = ((MultiRTSPClient*)clientData)->scs; // alias

  // Check each subsession, counting up how many packets have been received:
  unsigned newTotNumPacketsReceived = 0;

  MediaSubsessionIterator iter(*scs.session);
  MediaSubsession* subsession;
  while ((subsession = iter.next()) != NULL) {
    RTPSource* src = subsession->rtpSource();
    if (src == NULL) continue;
    newTotNumPacketsReceived += src->receptionStatsDB().totNumPacketsReceived();
  }

  if (newTotNumPacketsReceived == totNumPacketsReceived) {
    // No additional packets have been received since the last time we
    // checked, so end this stream:
    D("Closing session because we stopped receiving packets");
    scs.interPacketGapCheckTimerTask = NULL;
    shutdownStream((MultiRTSPClient*)clientData, LIVERTSP_EXIT_SERVER_NOT_RESPONDING);
  } else {
    totNumPacketsReceived = newTotNumPacketsReceived;
    // Check again, after the specified delay:
    scs.interPacketGapCheckTimerTask = env->taskScheduler().scheduleDelayedTask(LIVERTSP_INTERPACKET_GAP_TIME * 1000000, (TaskFunc*)checkInterPacketGaps, clientData);
  }
}