#!/bin/sh
PATH=/bin:/sbin:/usr/bin:/usr/sbin

eval "`/opt/em/bin/em-rec --dump-config`"

MPL_PIDS="`/usr/bin/pgrep mplayer`"

if [ ${?} -ne 0 ]; then
	echo "{ \"numProcs\": 0, \"cpuUsage\": 0 }"
	exit
fi

MPL_PIDS_CSV="`echo ${MPL_PIDS} | tr " " ","`"
CPU_USAGE=`top -bn 1 -p ${MPL_PIDS_CSV} | tail -n+8 | awk '{ s+=$9 } END { print s };'`

echo "{ \"numProcs\": `echo "$MPL_PIDS" | wc -l`, \"cpuUsage\": $CPU_USAGE }"
