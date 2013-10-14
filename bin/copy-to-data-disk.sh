#!/bin/bash
PATH=/bin:/sbin:/usr/bin:/usr/sbin
LAST_DUMP="/var/log/journal/last_dump.date"

source /opt/em/bin/read-config.func

mountpoint -q ${DATA_DISK}

if [ "$?" != "0" ]; then
	exit 0
fi

mkdir -p ${DATA_DISK}/reports
mkdir -p ${DATA_DISK}/screenshots
mkdir -p ${DATA_DISK}/video

# power up
touch ${DATA_DISK}/

# find all TRACK_*.csv / RFID / SYSTEM files
TRACK_FILES=`find ${OS_DISK} -maxdepth 1 -name "TRACK_*.csv"  | sort`
if [ -n "${TRACK_FILES}" ]; then
    cp -p --remove-destination ${TRACK_FILES} ${DATA_DISK}/
    if [ ${?} -eq 0 ]; then
        # remove last item from list and move rest to archived
        TRACK_FILES=`echo ${TRACK_FILES} | sed 's/[[:alnum:]\/\._\-]*$//'`
        if [ -n "${TRACK_FILES}" ]; then
            mv -f ${TRACK_FILES} ${OS_DISK}/archived/
        fi
    else
        echo "Copying TRACK files failed, didn't archive"
    fi
fi

SCAN_FILES=`find ${OS_DISK} -maxdepth 1 -name "SCAN_*.csv" | sort`
if [ -n "${SCAN_FILES}" ]; then
    cp -p --remove-destination ${SCAN_FILES} ${DATA_DISK}/
    if [ ${?} -eq 0 ]; then
        SCAN_FILES=`echo ${SCAN_FILES} | sed 's/[[:alnum:]\/\._\-]*$//'`
        if [ -n "${SCAN_FILES}" ]; then
            mv -f ${SCAN_FILES} ${OS_DISK}/archived/
        fi
    else
        echo "Copying SCAN files failed, didn't archive"
    fi
fi

SYSTEM_FILES=`find ${OS_DISK} -maxdepth 1 -name "SYSTEM_*.csv" | sort`
if [ -n "${SYSTEM_FILES}" ]; then
    cp -p --remove-destination ${SYSTEM_FILES} ${DATA_DISK}/
    if [ ${?} -eq 0 ]; then
        SYSTEM_FILES=`echo ${SYSTEM_FILES} | sed 's/[[:alnum:]\/\._\-]*$//'`
        if [ -n "${SYSTEM_FILES}" ]; then
            mv -f ${SYSTEM_FILES} ${OS_DISK}/archived/
        fi
    else
        echo "Copying SYSTEM files failed, didn't archive"
    fi
fi

# reports, screenshots
REPORT_FILES=`find ${OS_DISK}/reports -maxdepth 1 -name "*.txt" | sort`
if [ -n "${REPORT_FILES}" ]; then
    cp -p --remove-destination ${REPORT_FILES} ${DATA_DISK}/reports/
    if [ ${?} -eq 0 ]; then
        mv -f ${REPORT_FILES} ${OS_DISK}/archived/
    else
        echo "Copying reports failed, didn't archive"
    fi
fi

SCREENSHOT_FILES=`find ${OS_DISK}/screenshots -maxdepth 1 -name "*.jpg" | sort | head -n-1`
if [ -n "${SCREENSHOT_FILES}" ]; then
    cp -p --remove-destination ${SCREENSHOT_FILES} ${DATA_DISK}/screenshots/
    if [ ${?} -eq 0 ]; then
        mv -f ${SCREENSHOT_FILES} ${OS_DISK}/archived/
    else
        echo "Copying screenshots failed, didn't archive"
    fi
fi

# finally videos
VIDEO_FILES=`find ${OS_DISK}/video -maxdepth 1 -name "*.h264" | sort`
if [ -n "${VIDEO_FILES}" ]; then
    cp -p --remove-destination ${VIDEO_FILES} ${DATA_DISK}/video/
    if [ ${?} -eq 0 ]; then
        rm -f ${VIDEO_FILES}
    else
        echo "Copying videos failed, didn't remove"
    fi
fi

# now dump the systemd journal
DATE=`date "+%Y-%m-%d %H:%M:%S"`
NICE_DATE=`date "+%Y%m%d-%H%M%S"`

if [ -e ${LAST_DUMP} ]; then
    PREV_DATE=`cat ${LAST_DUMP}`
    EXTRA_PARAMS="--since=${PREV_DATE}"
fi

# misc
diff -q ${EM_DIR}/em.conf ${DATA_DISK}/em.conf > /dev/null 2>&1
if [ ${?} -ne 0 ]; then
    cp -p --remove-destination ${EM_DIR}/em.conf ${DATA_DISK}/
fi

IFS='%'
journalctl -a --output=short ${EXTRA_PARAMS} > ${DATA_DISK}/journal-${NICE_DATE}.log
unset IFS
echo -n ${DATE} > ${LAST_DUMP}

sync
sync

# power down
hdparm -y /dev/em_data > /dev/null 2>&1

# find all files in archived/ older than 2 weeks and remove
find ${OS_DISK}/archived -mtime +14 | xargs rm -rf
