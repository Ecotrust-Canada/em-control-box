#!/bin/bash
PATH=/bin:/sbin:/usr/bin:/usr/sbin
FFMPEG="/usr/bin/ffmpeg"
FFMPEG_OPTS="-loglevel warning -y -an -f rawvideo -c:v rawvideo -s 640x480 -r 30000/1001 -i /dev/cam0 -an -c:v libx264 -profile:v high -movflags faststart -threads 2 -level 42"
ENCODING_OPTS="-vf fps=30000/3003 -x264opts crf=24:subq=2:weightp=2:keyint=60:frameref=1:rc-lookahead=10:trellis=0:me=hex:merange=8 -maxrate 1200000 -bufsize 1300000"
LENGTH=3600

source /opt/em/bin/read-config.func

if [ -r "${EM_DIR}/encoding.conf" ]; then
    source ${EM_DIR}/encoding.conf
fi

while [ 1 ]; do
	if [ -e ${PAUSE_MARKER} ]; then
		sleep 10
	else
		COUNTER=${DATA_DISK}/hour_counter.dat
		DATE=`date "+%Y%m%d-%H%M%S"`

		if [ -e ${COUNTER} ]; then
			HOURS=`cat ${COUNTER}`
		else
			HOURS=0
		fi

		${FFMPEG} ${FFMPEG_OPTS} ${ENCODING_OPTS} -t ${LENGTH} /var/em/video/${DATE}.mp4
		HOURS=$(( $HOURS + 1 ))
		echo ${HOURS} > ${COUNTER}

		if [ ${?} -ne 0 ]; then
			exit 1
		fi
	fi
done
