#!/bin/sh
PATH=/bin:/sbin:/usr/bin:/usr/sbin
EXPECTED_MOZ=1
EXPECTED_MPL=1

sleep 65

eval "`/opt/em/bin/em-rec --dump-config`"

if [ "${cam}" -gt "0" ]; then
    EXPECTED_MOZ=$(($EXPECTED_MOZ * $cam))
    EXPECTED_MPL=$(($EXPECTED_MPL * $cam * 3))
fi

while [ 1 ]; do
	if [ "$(/usr/bin/pgrep -f /usr/bin/mozplugger-helper | wc -l)" -eq "${EXPECTED_MOZ}" -a "$(/usr/bin/pgrep -f /usr/bin/mplayer | wc -l)" -eq "${EXPECTED_MPL}" ]; then
		continue
	fi

	killall firefox
	echo check-browser restarted firefox
	sleep 30
done
