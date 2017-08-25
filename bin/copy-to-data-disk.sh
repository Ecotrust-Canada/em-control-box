#!/bin/bash
PATH=/bin:/sbin:/usr/bin:/usr/sbin
LAST_DUMP="/var/log/journal/last_dump.date"

eval "`/opt/em/bin/em-rec --dump-config`"

mountpoint -q ${DATA_DISK}

if [ "$?" != "0" ]; then
	exit 0
fi

# power up
echo powering up
touch ${DATA_DISK}/

echo making dirs
mkdir -p ${OS_DISK}/archived
mkdir -p ${DATA_DISK}/elog
mkdir -p ${DATA_DISK}/journal
mkdir -p ${DATA_DISK}/reports
mkdir -p ${DATA_DISK}/screenshots
mkdir -p ${DATA_DISK}/video

echo copy track
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

echo copy scan
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

echo copy system
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

echo copy other
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

# I can't remember why I did head -n-1 here
SCREENSHOT_FILES=`find ${OS_DISK}/screenshots -maxdepth 1 -name "*.jpg" | sort | head -n-1`
if [ -n "${SCREENSHOT_FILES}" ]; then
    cp -p --remove-destination ${SCREENSHOT_FILES} ${DATA_DISK}/screenshots/
    if [ ${?} -eq 0 ]; then
        mv -f ${SCREENSHOT_FILES} ${OS_DISK}/archived/
    else
        echo "Copying screenshots failed, didn't archive"
    fi
fi

#Copy all the home port screenshots
echo copy homeport screenshots
TODAY_DATE=`date +%Y-%m-%d`
YEST_DATE=`date -d "yesterday 13:00" +%Y-%m-%d`
echo "todays date: ${TODAY_DATE} Yesterdays: ${YEST_DATE}"
SCREENSHT_FOLDERS=`find ${OS_DISK} -name "${TODAY_DATE}" -o -name "${YEST_DATE}"`
if [ -n "${SCREENSHT_FOLDERS}" ]; then
    cp -p -r --remove-destination ${SCREENSHT_FOLDERS} ${DATA_DISK}
    if [ ${?} -eq 0 ]; then
        rm -rf ${SCREENSHT_FOLDERS}
    else
        echo "Copying homeport screenshots failed"
    fi
fi

echo copy videos
function copy_videos {
	# finally videos
	VIDEO_FILES=`find ${OS_DISK}/video -maxdepth 1 -name "*-cam${1}.mp4" | sort`
	VIDEO_FILES=`echo ${VIDEO_FILES} | sed 's/[[:alnum:]\/\._\-]*$//'`
	if [ -n "${VIDEO_FILES}" ]; then
	    cp -p --remove-destination ${VIDEO_FILES} ${DATA_DISK}/video/
	    if [ ${?} -eq 0 ]; then
	        rm -f ${VIDEO_FILES}
	    else
	        echo "Copying videos failed, didn't remove"
	    fi
	fi
}

copy_videos 0
copy_videos 1
copy_videos 2
copy_videos 3
copy_videos 4

echo copy elog
# elog
cp -Rp --remove-destination /var/elog/* ${DATA_DISK}/elog/

echo copy journal
# now dump the systemd journal
DATE=`date "+%Y-%m-%d %H:%M:%S"`
NICE_DATE=`date "+%Y%m%d-%H%M%S"`

if [ -e ${LAST_DUMP} ]; then
    PREV_DATE=`cat ${LAST_DUMP}`
    if [ -z "${PREV_DATE}" ]; then
        EXTRA_PARAMS=""
    else
        EXTRA_PARAMS="--since=${PREV_DATE}"
    fi
    
fi

# misc
diff -q ${EM_DIR}/em.conf ${DATA_DISK}/em.conf > /dev/null 2>&1
if [ ${?} -ne 0 ]; then
    cp -p --remove-destination ${EM_DIR}/em.conf ${DATA_DISK}/
fi

echo actually copy journal here
IFS='%'
journalctl -a --output=short ${EXTRA_PARAMS} > ${DATA_DISK}/journal/${NICE_DATE}.log
unset IFS
echo -n ${DATE} > ${LAST_DUMP}

echo sync
sync
sync

echo power down
# power down
hdparm -y /dev/em_data > /dev/null 2>&1

echo archive
# find all files in archived/ older than 21 days and remove
find ${OS_DISK}/archived -mtime +21 | xargs rm -rf
