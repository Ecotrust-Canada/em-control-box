#!/bin/bash
PATH=/bin:/sbin:/usr/bin:/usr/sbin

EM_CONF="/etc/em.conf"
LAST_DUMP="/var/log/journal/last_dump.date"

if [ ! -r ${EM_CONF} ]; then
        echo "Conf file ${EM_CONF} not readable!"
        exit 1
fi

if [ ! -r /tmp/.em.conf ]; then
	cat ${EM_CONF} | sed 's/=\(.*\)/="\1"/g' > /tmp/.em.conf
fi

source /tmp/.em.conf

mountpoint -q ${DATA_MNT}

if [ "$?" != "0" ]; then
	exit 0
fi

DATE=`date "+%Y-%m-%d %H:%M:%S"`
NICE_DATE=`date "+%Y%m%d-%H%M%S"`

if [ -e ${LAST_DUMP} ]; then
	PREV_DATE=`cat ${LAST_DUMP}`
	EXTRA_PARAMS="--since=${PREV_DATE}"
fi

cp -RpH /etc/em.conf ${BACKUP_DATA_MNT}/report-* ${DATA_MNT}/ > /dev/null 2>&1
IFS='%'
journalctl -a --output=short ${EXTRA_PARAMS} > ${DATA_MNT}/journal-${NICE_DATE}.log
unset IFS
echo -n ${DATE} > ${LAST_DUMP}

