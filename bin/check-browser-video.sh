#!/bin/sh
PATH=/bin:/sbin:/usr/bin:/usr/sbin

eval "`/opt/em/bin/em-rec --dump-config`"

EXPECTED_MOZ=$(($cam * 1))
EXPECTED_MPL=$(($cam * 3))
EXPECTED_CPU=$(($cam * 25)) # at least 25% per cam to play the stream
MOZ_PIDS="`/usr/bin/pgrep -f /usr/bin/mozplugger-helper`"
MPL_PIDS="`/usr/bin/pgrep -f /usr/bin/mplayer`"
MPL_PIDS_CSV="`echo ${MPL_PIDS} | tr " " ","`"
CPU_USAGE=`top -bn 1 -p ${MPL_PIDS_CSV} | tail -n+8 | awk '{ s+=$9 } END { print s };'`

if [ "$(echo "${MOZ_PIDS}" | wc -l)" -eq "${EXPECTED_MOZ}" -a "$(echo "${MPL_PIDS}" | wc -l)" -eq "${EXPECTED_MPL}" -a "$(echo ${CPU_USAGE}'>'${EXPECTED_CPU} | bc -l)" -eq "1" ]; then
    exit
fi

echo check-browser-video.sh restarting Firefox
killall firefox
