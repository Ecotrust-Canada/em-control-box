#!/bin/sh
PATH=/bin:/sbin:/usr/bin:/usr/sbin

sleep 30

while [ 1 ]; do
	sleep 30

	if [ "$(/usr/bin/pgrep -n -f /usr/bin/mozplugger-helper)" -a "$(/usr/bin/pgrep -n -f /usr/bin/mplayer)" ]; then
		continue
	fi

	killall firefox
done
