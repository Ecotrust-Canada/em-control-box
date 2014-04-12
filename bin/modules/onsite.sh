NAME="clear flashard format monitor play resetgps start stop upgrade fixethernet"

stop_description="Stops all EM services"
stop_start() {
	echo -ne "      ${STAR} Stopping all EM services ... " &&
	/bin/systemctl stop smartctl.timer sensors.timer startx.service web-server.service elog-server.service em-rec.service capture-bttv.service gpsd.service
	echo -e ${OK}
}

start_description="Starts all EM services"
start_start() {
	echo -ne "      ${STAR} Starting all EM services " &&
	/bin/systemctl start capture-bttv.service gpsd.service smartctl.timer sensors.timer
	echo -n .
	/bin/systemctl start em-rec.service
	echo -n .
	/bin/systemctl start web-server.service elog-server.service startx.service
	echo -n .
	echo -e " ${OK}"
}

check_var_safety() {
	if [ -z "${DATA_DISK}" ]; then
		echo -e "${bldred}DATA_DISK not properly set in em.conf${txtrst}"
		exit 1
	fi

	if [ -z "${OS_DISK}" ]; then
		echo -e "${bldred}OS_DISK not properly set in em.conf${txtrst}"
		exit 1
	fi
}

flashard_description="Flashes Arduino with analog data collector image"
flashard_start() {
	if [ ${#} -eq 1 ]; then
		arduino=${1}
	fi

	if [ "${arduino}" == "3.3V" -o "${arduino}" == "3.3VD" ]; then
		BOARD="pro328"
		AVRDUDE_OPTS="-p atmega328p -c arduino"
	elif [ "${arduino}" == "5V" ]; then
		BOARD="pro5v328"
		AVRDUDE_OPTS="-p atmega328p -c arduino"
	elif [ "${arduino}" == "5VPM" ]; then
		BOARD="promicro16"
		AVRDUDE_OPTS="-p atmega32u4 -c avr109"

		echo -ne "	${STAR} Resetting Pro Micro ... " &&
		stty -F ${ARDUINO_DEV} 1200 &&
		sleep 3 &&
		echo -e ${OK}
	else
		echo -e "${bldred}Arduino type in /etc/em.conf not set or invalid${txtrst}" &&
		exit 1
	fi

	echo -e "	${STAR} Flashing Arduino ${BOARD} ... " &&
	avrdude -q -V -b 57600 -P ${ARDUINO_DEV} ${AVRDUDE_OPTS} -U flash:w:/opt/em/arduino/${BOARD}.hex:i
	
}

resetgps_description="Resets Garmin GPS to known good configuration"
resetgps_start() {
	#systemctl stop gpsd.service

	echo -ne "	${STAR} Configuring serial ports again (just to make sure) ... "
	setserial -z ${GPS_DEV} low_latency
	setserial -z ${RFID_DEV} low_latency
	if [ "${fishing_area}" == "A" ]; then
		stty -F ${GPS_DEV} 4800
	else
		stty -F ${GPS_DEV} 38400
	fi
	echo -e ${OK}

	echo -ne "	${STAR} Sending sensor configuration ... " &&
	if [ "${fishing_area}" == "A" ]; then
		echo -ne '$PGRMC,A,,,,,,,,A,3,1*65\r\n' > ${GPS_DEV}
		sleep 2
		echo -ne '$PGRMC,A,,,,,,,,A,3,1,,,1*78\r\n' > ${GPS_DEV}
		sleep 2
	else
		echo -ne '$PGRMC,A,,,,,,,,A,8,1,,,1*73\r\n' > ${GPS_DEV}
		sleep 2
		echo -ne '$PGRMC2,5,LOW,,,,,0*04\r\n' > ${GPS_DEV}
		sleep 2
	fi

	echo -ne '$PGRMC1,1,1,2,,,,2,W,N,1,1,1*52\r\n' > ${GPS_DEV}
	sleep 2
	echo -e ${OK}

	echo -ne "	${STAR} Disabling all output sentences ... " &&
	echo -ne '$PGRMO,,2*75\r\n' > ${GPS_DEV}
	sleep 2
	echo -e ${OK}

	echo -ne "	${STAR} Enabling GPGGA, GPGSV, GPGSA, GPRMC, PGRME ... "
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

	#systemctl start gpsd.service
}

clear_description="Clears the data disk"
clear_start() {
	check_var_safety

	echo -ne "	${STAR} WARNING: Are you sure you want to clear ALL EM data (except em.conf)? (Y/N) "
	read answer
	while [ "${answer}" != "Y" -a "${answer}" != "N" ]; do
		echo -n "Please input Y or N: "
		read answer
	done

	if [ "${answer}" == "Y" ]; then
		stop_start

		echo -ne "	${STAR} Clearing data disk ... " &&
		rm -Rf ${DATA_DISK}/* &&
		echo -e ${OK}

		echo -ne "	${STAR} Clearing OS disk ... " &&
		rm -Rf ${OS_DISK}/* &&
		echo -e ${OK}
	fi
}

format_description="Partitions, formats, labels, and prepares a new data disk"
format_usage="
Usage:\t${bldwht}em format\t\t\t${txtrst}(Easy prompts for device and EM disk number)\n
\t${bldwht}em format <device> <emdisk#>\t${txtrst}(No prompts, expert use)\n
\t\tex: em format sdb 154"
format_start() {
	if [ ${#} -eq 0 ]; then
		while read DEV; do
			TOKENS=(${DEV})
			#if [ "${TOKENS[0]}" == "sda" ]; then continue; fi

			if [ "${TOKENS[1]}" -ge 80000000000 ]; then
				DEVICE=${TOKENS[0]}
				echo -e "${txtgrn}Found a block device >=80 GB with model ${TOKENS[@]:2} at /dev/${DEVICE}${txtrst}"

				break
			fi
		done < <(lsblk -bdnro NAME,SIZE,MODEL)

		echo -ne "${bldwht}What disk would you like to format? ${txtrst}"
		if [ -n "${DEVICE}" ]; then
			echo -n "[/dev/${DEVICE}] (Enter to accept default) "
		fi

		read _DEVICE
		if [ -n "${_DEVICE}" ]; then
			DEVICE=${_DEVICE}
		fi

		tmp_DEVICE="/dev/`basename ${DEVICE}`"
		LABEL="`lsblk -nro LABEL ${tmp_DEVICE}1 2>/dev/null`"

		if [[ ${LABEL} == DATA_* ]]; then
			LABEL="${LABEL##DATA_}"
		fi

		echo -ne "${bldwht}What is this disk's assigned number? ${txtrst}"
		if [ -n "${LABEL}" ]; then
			echo -n "[${LABEL}] (Enter to accept current label) "
		fi

		read _NUMBER
		if [ -n "${_NUMBER}" ]; then
			NUMBER=${_NUMBER}
		elif [ -z "${_NUMBER}" -a -z "${LABEL}" ]; then
			echo -e "${bldred}No disk number, no format! Sorry!${txtrst}"
			exit 1
		else
			NUMBER=${LABEL}
		fi
	elif [ ${#} -eq 2 ]; then
		DEVICE=${1}
		NUMBER=${2}
	else
		echo -e ${format_usage} &&
		exit 1
	fi

	if [ ${NUMBER} -eq ${NUMBER} 2> /dev/null ]; then
		format_DEVICE="/dev/`basename ${DEVICE}`"
		format_LABEL="DATA_${NUMBER}"

	else
		echo -e "${bldred}Disk number can only be a number!${txtrst}"
		exit 1
	fi

	echo -e "${bldred}Beginning operations on ${format_DEVICE} in 3 seconds ... hit CTRL+C to stop me${txtrst}"
	sleep 3

	echo -ne "	${STAR} Unmounting ${format_DEVICE} ... " &&
	sudo umount -f ${format_DEVICE} > /dev/null 2>&1
	echo -e ${OK}

	echo -ne "	${STAR} Clearing partition table ... " &&
	sudo dd if=/dev/zero of=${format_DEVICE} bs=4096 count=1 > /dev/null 2>&1 &&
	echo -e ${OK}

	echo -ne "	${STAR} Creating a new partition ... " &&
	sudo echo "2048,,L" | sfdisk -uSqL ${format_DEVICE} > /dev/null 2>&1 &&
	echo -e ${OK}

	echo -ne "	${STAR} Formatting and labelling the partition ... " &&
	mkfs.ext4 -q -L ${format_LABEL} -O none,dir_index,extent,filetype,flex_bg,has_journal,sparse_super,uninit_bg,large_file,extra_isize -E discard -b 4096 -I 256 -i 32768 -m 1 ${format_DEVICE}1 &&
	echo -e ${OK}

	echo -ne "	${STAR} Triggering udev events ... " &&
	udevadm trigger --action=add &&
	sleep 2 &&
	echo -e ${OK}
}

monitor_description="Starts serial monitor to listen to device output"
monitor_usage="
Usage:\t${bldwht}em monitor <device>\t\t${txtrst}(where <device> is 'gps' or 'rfid' or 'arduino')\n
\tex: em monitor gps"
monitor_start() {
	case "${1}" in
		"gps")
			/usr/bin/gpsmon ${GPS_DEV}
			exit 0
			;;
		"rfid")
			DEVICE=${RFID_DEV}
			BAUD=9600
			;;
		"arduino")
			DEVICE=${ARDUINO_DEV}
			BAUD=9600
			;;
		*)
			echo -e ${monitor_usage}
			exit 1
			;;
	esac

	echo -e "To quit: ${bldgrn}CTRL+A k y${txtrst}"
	sleep 3
	screen ${DEVICE} ${BAUD}
}

play_description="Plays a movie file on the UI display"
play_usage="
Usage:\t${bldwht}em play <file>\t\t${txtrst}(where <file> is a .mp4 or .h264 file)\n
\tex: em play /mnt/data/20130802-184800.h264"
play_start() {
	if [ ${#} -ne 1 ]; then
		echo -e ${play_usage}
		exit 1
	fi

	export DISPLAY=:0
	echo -e "	${STAR} Playing ${1} ... "
	echo
	echo -e "	${bldgrn}Switch to the UI to see it (usually CTRL+ALT+F2)"
	echo -e "	${bldred}To stop playing press CTRL+C on this screen${txtrst}"
	mplayer -msglevel all=1 -vf scale=1024:-3 -xy 1024 ${1}

	if [ ${?} -eq 0 ]; then
		echo -e "\n${txtgrn}Finished${txtrst}"
	fi
}

upgrade_description="Upgrade system to new EM image"
upgrade_usage="
Usage:\t${bldwht}em upgrade <image>\t\t${txtrst}(where <image> is the new image file)\n
\tex: em upgrade /mnt/data/em-2.0.1"
upgrade_start() {
	if [ ${#} -ne 1 ]; then
		echo -e ${upgrade_usage}
		exit 1
	fi

	if [ ${UID} -ne 0 ]; then
		echo -e "${bldred}You must be root to use this command${txtrst}"
		exit 1
	fi

	IMAGE=${1}
	if [ ! -f ${IMAGE} ]; then
		echo -e "${bldred}Image ${bldwht}${IMAGE}${bldred} can't be found${txtrst}"
		exit 1
	fi

	RELEASE="${IMAGE##*-}"
	echo -ne "	${STAR} Mounting /boot ... " &&
	if ! mountpoint -q /boot; then
		mount /dev/disk/by-label/BOOT /boot
	fi
	echo -e ${OK}

	echo -ne "	${STAR} Installing ${IMAGE} ... " &&
	cp ${IMAGE} /boot/
	echo -e ${OK}

	echo -ne "	${STAR} Updating GRUB ... " &&
	if ! grep -q ${RELEASE} /boot/grub/grub.cfg; then
		echo "menuentry 'EM ${RELEASE}' --class electronic --class gnu-linux --class gnu --class os --unrestricted \$menuentry_id_option 'em-${RELEASE}' {" >> /boot/grub/grub.cfg
		echo "savedefault" >> /boot/grub/grub.cfg
		echo "insmod gzio" >> /boot/grub/grub.cfg
		echo "insmod part_msdos" >> /boot/grub/grub.cfg
		echo "insmod ext2" >> /boot/grub/grub.cfg
		echo "set root=(hd0,1)" >> /boot/grub/grub.cfg
		echo "echo 'Loading EM software v${RELEASE} ...'" >> /boot/grub/grub.cfg
		echo "linux /em-${RELEASE} ro quiet" >> /boot/grub/grub.cfg
		echo "}" >> /boot/grub/grub.cfg
		echo >> /boot/grub/grub.cfg
	fi
	echo -e ${OK}

	echo -ne "	${STAR} Unmounting /boot ... " &&
	umount /boot
	echo -e ${OK}
}

fixethernet_description="Fixes Intel Ethernet EEPROM (run once for every fresh new-generation motherboard)"
fixethernet_start() {
	echo -ne "	${STAR} Writing bits to Ethernet EEPROM ... " &&
	/usr/sbin/ethtool -E eth0 magic 0x10d38086 offset 0x1e value 0x5a
	echo -e "${OK}"
	echo
	echo "Changes will take effect on next power cycle"
}
