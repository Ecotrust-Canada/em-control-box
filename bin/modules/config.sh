NAME="wizard config resetgps"
wziard_description="Setup wizard for em.conf"
config_description="Edit em.conf file by hand"
resetgps_description="Resets Garmin GPS to known good configuration"

wizard_start() {
	echo "Not implemented"
}

config_start() {
	vim ${EM_CONF}
}

resetgps_start() {
	stop_start

	echo -ne "	${STAR} Configuring serial ports again (just to make sure) ... "
	setserial -z ${GPS_DEV} low_latency
	setserial -z ${RFID_DEV} low_latency
	stty -F ${GPS_DEV} 4800
	stty -F ${RFID_DEV} 9600
	echo -e ${OK}

	echo -ne "	${STAR} Sending sensor configuration ... " &&
	echo -ne '$PGRMC,A,,,,,,,,A,3,1,,,*49\r\n' > ${GPS_DEV}
	sleep 2
	echo -ne '$PGRMC1,1,1,2,,,,2,W,N,1,1,1*52\r\n' > ${GPS_DEV}
	sleep 2
	echo -e ${OK}

	echo -ne "	${STAR} Disabling all output sentences ... " &&
	echo -ne '$PGRMO,,2*75\r\n' > ${GPS_DEV}
	sleep 2
	echo -e ${OK}

	echo -ne "	${STAR} Enabling GPGGA, GPGSV, GPGSA, GPRMC ... "
	echo -ne '$PGRMO,GPGGA,1*20\r\n' > ${GPS_DEV}
	sleep 2
	echo -ne '$PGRMO,GPGSV,1*23\r\n' > ${GPS_DEV}
	sleep 2
	echo -ne '$PGRMO,GPGSA,1*34\r\n' > ${GPS_DEV}
	sleep 2
	echo -ne '$PGRMO,GPRMC,1*3D\r\n' > ${GPS_DEV}
	sleep 2
	echo -ne '$PGRMO,PGRME,1*3B\r\n' > ${GPS_DEV}
	sleep 2
	echo -e ${OK}

	echo -ne "	${STAR} Resetting GPS ... "
	echo -ne '$PGRMI,,,,,,,R\r\n' > ${GPS_DEV}
	echo -e ${OK}

	echo
	echo -e "Remember to ${bldgrn}em start${txtrst} if it was running before!"
}
