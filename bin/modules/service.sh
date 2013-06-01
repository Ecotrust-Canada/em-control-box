NAME="start stop restart pushlogs"
start_description="Starts EM recording"
stop_description="Stops EM recording"
restart_description="Restarts EM recording"
pushlogs_description="Pushes EM system logs to data disk"

pushlogs_start() {
	stop_start
	sleep 1

	echo -ne "	${STAR} Copying system logs ..." &&
	# test if data disk mounted, error out if not
	mkdir -p ${DATA_MNT}/logs
	cat ${EM_LOG} >> ${DATA_MNT}/logs/em.log
	cat ${EM_ERROR_LOG} >> ${DATA_MNT}/logs/em.err
	#cat ${EM_FOREVER_LOG} >> ${DATA_MNT}/logs/em.forever
	cp /var/log/syslog* ${DATA_MNT}/logs/
	echo -e ${OK}

	echo -ne "	${STAR} Removing old logs ... " &&
	rm -f ${EM_LOG} > /dev/null 2>&1
	rm -f ${EM_ERRORLOG} > /dev/null 2>&1
	#rm -f ${EM_FOREVERLOG} > /dev/null 2>&1
	echo -e ${OK}

	echo -ne "	${STAR} Backing up other data ... " &&
	cp ${EM_DIR}/* ${DATA_MNT}/ &&
	echo -e ${OK}

	start_start
}
