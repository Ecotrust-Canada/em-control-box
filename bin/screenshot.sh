#!/bin/bash
PATH=/bin:/sbin:/usr/bin:/usr/sbin

EM_CONF="/etc/em.conf"
INTERVAL=600
export DISPLAY=":0"

if [ ! -r ${EM_CONF} ]; then
        echo "Conf file ${EM_CONF} not readable!"
        exit 1
fi

if [ ! -r /tmp/.em.conf ]; then
	cat ${EM_CONF} | sed 's/=\(.*\)/="\1"/g' > /tmp/.em.conf
fi

source /tmp/.em.conf

if [ -z ${SCREENSHOT_MARKER} ]; then
	exit 1
fi

sleep 30

while [ 1 ]; do
	if [ -e ${SCREENSHOT_MARKER} ]; then
		mkdir -p /var/em/video/screenshots
		scrot -q25 /var/em/video/screenshots/screenshot-%Y%m%d-%H%M%S.jpg
		sleep ${INTERVAL}
	else
		sleep 5
	fi
done

