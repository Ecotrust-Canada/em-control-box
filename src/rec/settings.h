#ifndef SETTINGS_H
#define SETTINGS_H

#include "states.h"

#define FN_CONFIG				"/etc/em.conf"
#define FN_TRACK_LOG			"TRACK"
#define FN_SCAN_LOG				"SCAN"
#define FN_SYSTEM_LOG			"SYSTEM"
#define FN_SCAN_COUNT			"scan_count.dat"
#define FN_VIDEO_DIR			"/video"
#define FN_REC_COUNT            "rec_count.dat"

#define NOTIFY_SCAN_INTERVAL		6	 	// wait this many POLL_PERIODs between honks
#define RECORD_SCAN_INTERVAL		300		// 300 P_Ps = 5 minutes
#define USE_WARNING_HONK			false
#define DATA_DISK_FAKE_DAYS_START	21
#define SCREENSHOT_STATE_DELAY      120     // screenshot state has to be around continuously for this many seconds before it is exposed
#define DISK_USAGE_SAMPLES          12
#define MAX_CLIP_LENGTH             1800    // force new movie file every x seconds
//#define MAX_CLIP_LENGTH             20    // force new movie file every x seconds

#define FFMPEG_BINARY               "/usr/bin/ffmpeg"
#define FFMPEG_MAX_ARGS             48
#define FFMPEG_ARGS_LINE            "-loglevel warning -y -an -f rawvideo -c:v rawvideo -s %s -r %s -i %s -an -c:v libx264 -profile:v high -movflags faststart -threads 2 -level 42 -vf fps=%s -x264opts %s -maxrate %s -bufsize %s -t" // -t HAS to be the last parameter

#define ANALOG_MAX_CAMS             4
#define ANALOG_INPUT_RESOLUTION     "640x480"
#define ANALOG_INPUT_FPS            "30000/1001" // 29.97
#define ANALOG_INPUT_DEVICE         "/dev/cam0"
#define ANALOG_OUTPUT_FPS_NORMAL    "30000/3003" // 1/3 of 29.97 (~10)
#define ANALOG_OUTPUT_FPS_SLOW      "30000/15015" // 1/15 of 29.97 (~2)
#define ANALOG_H264_OPTS            "crf=24:subq=2:weightp=2:keyint=60:frameref=1:rc-lookahead=10:trellis=0:me=hex:merange=8"
#define ANALOG_MAX_RATE             "1200000"
#define ANALOG_BUF_SIZE             "1300000"

#define DIGITAL_MAX_CAMS            4
#define DIGITAL_RTSP_URL            "rtsp://1.1.1.%d:7070/track1"
#define DIGITAL_HTTP_API_COMMAND    "/usr/bin/wget -q -O - \"$@\" \"%s\""
#define DIGITAL_HTTP_API_URL        "http://1.1.1.%d/cgi-bin/encoder?USER=Admin&PWD=123456%s"
#define DIGITAL_HTTP_API_FPS        "&VIDEO_FPS_NUM="
#define DIGITAL_OUTPUT_WIDTH        1280
#define DIGITAL_OUTPUT_HEIGHT       720
#define DIGITAL_OUTPUT_FPS_NORMAL   15   // On Acti cameras, choices are 1, 3, 5, 10, 15, 30
#define DIGITAL_OUTPUT_FPS_SLOW     1

#define DEFAULT_fishing_area		"A"
#define DEFAULT_rfid          "yes"
#define DEFAULT_video         "digital"
#define DEFAULT_vessel        "NOT_CONFIGURED"
#define DEFAULT_vrn         "00000"
#define DEFAULT_arduino       "5V"
#define DEFAULT_psi_vmin      "0.95"
#define DEFAULT_psi_low_threshold   "50"
#define DEFAULT_psi_high_threshold  "650"
#define DEFAULT_fps_low_delay       "120"
#define DEFAULT_cam         "1"

#define DEFAULT_EM_DIR				"/var/em"
#define DEFAULT_OS_DISK				"/var/em/data"
#define DEFAULT_DATA_DISK 			"/mnt/data"
#define DEFAULT_JSON_STATE_FILE 	"/tmp/em_state.json"
#define DEFAULT_ARDUINO_DEV 		"/dev/arduino"
#define DEFAULT_GPS_DEV 			"/dev/ttyS0"
#define DEFAULT_RFID_DEV 			"/dev/ttyS1"
//#define DEFAULT_HOME_PORT_DATA 		"/opt/em/public/a_home_ports.kml"
//#define DEFAULT_FERRY_DATA			"/opt/em/public/a_ferry_lanes.kml"
#define DEFAULT_HOME_PORT_DATA 		"/need/to/set/this"
#define DEFAULT_FERRY_DATA			"/need/to/set/this"

#endif
