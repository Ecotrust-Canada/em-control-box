#!/bin/bash
PATH=/bin:/sbin:/usr/bin:/usr/sbin

EM_CONF="/etc/em.conf"
MENCODER="/usr/bin/mencoder"
ENCODING_OPTS_IP="-msglevel all=2 -rtsp-stream-over-tcp -ovc copy -of rawvideo "
LENGTH=1800

if [ ! -r ${EM_CONF} ]; then
        echo "Conf file ${EM_CONF} not readable!"
        exit 1
fi

if [ ! -r /tmp/.em.conf ]; then
	cat ${EM_CONF} | sed 's/=\(.*\)/="\1"/g' > /tmp/.em.conf
fi

source /tmp/.em.conf

if [ -z "${CAMERASTREAM}" ]; then
	echo You must specify a stream \(1/2/3/4\) to capture by setting the env var CAMERASTREAM
	exit 1
fi

if [ "${CAMERASTREAM}" -gt "${cam}" ]; then
	echo Stream specified is not active. Exiting ...
	systemctl stop dump-rtsp-stream-${CAMERASTREAM}.service
	sleep 5
	exit 0
fi

if [ -r "${EM_DIR}/encoding.conf" ]; then
	source ${EM_DIR}/encoding.conf
fi

while [ 1 ]; do
	if [ -e ${PAUSE_MARKER} ]; then
		sleep 10
	else
		#COUNTER=${DATA_MNT}/hour_counter.dat
		DATE=`date "+%Y%m%d-%H%M%S"`

		#if [ -e ${COUNTER} ]; then
		#	HOURS=`cat ${COUNTER}`
		#else
		#	HOURS=0
		#fi

		${MENCODER} ${ENCODING_OPTS_IP} -endpos ${LENGTH} -o /var/em/video/${DATE}-cam${CAMERASTREAM}.h264 rtsp://1.1.1.${CAMERASTREAM}:7070/track1
		#HOURS=$(( $HOURS + 1 ))
		#echo ${HOURS} > ${COUNTER}

		#if [ ${?} -ne 0 ]; then
		#	exit 1
		#fi

		chown ecotrust:ecotrust /var/em/video/${DATE}-cam${CAMERASTREAM}.h264

		sleep 1
	fi
done
