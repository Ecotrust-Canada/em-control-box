NAME="clear flashard format monitor play resetgps start stop upgrade updategrub fixethernet resetelog mountusb savetousb screenres"

stop_description="Stops all EM services"
stop_start() {
	echo -ne "      ${STAR} Stopping all EM services ... " &&
	/bin/systemctl stop copy-to-data-disk.timer smartctl.timer sensors.timer startx.service web-server.service elog-server.service em-rec.service capture-bttv.service gpsd.service
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
	/bin/systemctl start copy-to-data-disk.timer
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
Usage:\t${bldwht}em upgrade <version>\t\t${txtrst}(where <version> is the new image version which you have on USB)\n
\tex: em upgrade 2.2.5"
upgrade_start() {
	if [ ${#} -ne 1 ]; then
		echo -e ${upgrade_usage}
		exit 1
	fi

	if [ ${UID} -ne 0 ]; then
		echo -e "${bldred}You must be root to use this command${txtrst}"
		exit 1
	fi

	mountusb_start

	if [ "${FOUND_FAT_PARTITION}" = "false" ]; then
		echo "Nothing to mount or upgrade"
		exit 1
	fi

	RELEASE=${1}

	if ! mountpoint -q /boot; then
		echo -ne "	${STAR} Mounting /boot ... " && systemctl start boot.mount && echo -e ${OK}
	fi

	echo -ne "	${STAR} Installing ${RELEASE} ... " &&
		if [ ! -f /mnt/usb/em-${RELEASE} ]; then
			echo -e "${bldred}Image ${bldwht}/mnt/usb/em-${RELEASE}${bldred} can't be found${txtrst}"
			exit 1
		fi
		cp /mnt/usb/em-${RELEASE} /boot/
	echo -e ${OK}

	updategrub_start

	echo -ne "	${STAR} Unmounting /boot ... " &&
		systemctl stop boot.mount
	echo -e ${OK}
}

updategrub_description="Regenerates GRUB boot menu with available EM images"
updategrub_usage="
Usage:\t${bldwht}em updategrub${txtrst}"
updategrub_start() {
	if ! mountpoint -q /boot; then
		echo -ne "	${STAR} Mounting /boot ... " && systemctl start boot.mount && echo -e ${OK}
	fi

	echo -ne "	${STAR} Regenerating GRUB menus ... " &&
		cp /opt/em/src/grub.cfg /boot/grub/grub.cfg
		for IMAGE in `ls /boot/em-*`; do
			RELEASE="${IMAGE##*-}"

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
		done
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

resetelog_description="Resets all elog data. Also restarts the elog and draws user credentials from /etc/em.conf anew."
resetelog_start() {
        rm /var/elog/elog.db/apportion.json*
        rm /var/elog/elog.db/catch.json*
        rm /var/elog/elog.db/effort.json*
        rm /var/elog/elog.db/landing.json*
        rm /var/elog/elog.db/effort.json*
        rm /var/elog/elog.db/usergear.json*
        rm /var/elog/elog.db/user.json*
        rm /var/elog/elog.db/trip.json*
        rm /var/elog/elog.db/gearpreset.json*
        systemctl restart elog-server.service
}

mountusb_description="Finds and mounts a USB flash drive"
mountusb_usage="
Usage:\t${bldwht}em mountusb${txtrst}"
mountusb_start() {
	FOUND_USB=false
	FOUND_FAT_PARTITION=false

	for DEV in /sys/block/sd*; do
		if readlink -f ${DEV}/device | grep -q usb; then
			DEV=`basename ${DEV}`
			echo "${DEV} is a USB device"
            FOUND_USB=true
			if [ -d /sys/block/${DEV}/${DEV}1 ]; then
				echo "${DEV} has partitions"
				if file -s /dev/${DEV}1 | grep -q FAT; then
					echo "${DEV}1 is a FAT partition, trying to mount"
					umount /dev/${DEV}1 > /dev/null 2>&1
					mkdir -p /mnt/usb
					if mount /dev/${DEV}1 /mnt/usb; then
						echo "${DEV}1 mounted"
						FOUND_FAT_PARTITION=true
					fi
				fi
			else
				echo "Has no partitions"
			fi
		fi
	done	
        
    if [ "${FOUND_USB}" = "false" ]; then
            echo "No USB devices found"
    fi
}


savetousb_description="Dumps specified file to USB flash drive in a forceful way"
savetousb_usage="
Usage:\t${bldwht}em savetousb <path/to/file>${txtrst}\n
\tex: em savetousb /tmp/12345678.csv"
savetousb_start() {
	if [ ${#} -ne 1 ]; then
		echo -e ${savetousb_usage}
		exit 1
	fi

	mountusb_start

	if [ "${FOUND_FAT_PARTITION}" = "false" ]; then
		echo -ne "	${STAR} Clearing partition table ... " &&
		dd if=/dev/zero of=/dev/${DEV} bs=4096 count=1 > /dev/null 2>&1 &&
		echo -e ${OK} && sleep 1 &&
		sfdisk -R /dev/${DEV} &&

		echo -ne "	${STAR} Creating new partition ... " &&
		echo -e "2048,,C" | sfdisk -uS -qL /dev/${DEV} > /dev/null 2>&1 &&
		echo -e ${OK} &&

		echo -ne "	${STAR} Formatting partition ... " &&
		mkfs.vfat /dev/${DEV}1 > /dev/null 2>&1 &&
		echo -e ${OK} &&

		mount /dev/${DEV}1 /mnt/usb &&
		echo "${DEV}1 mounted"

		if [ ${?} -ne 0 ]; then
			exit 1
		fi
	fi

	echo -e "  ${STAR} Copying and unmounting ... " &&
	cp -av ${1} /mnt/usb/ &&
	sync &&
	umount /dev/${DEV}1

	if [ ${?} -ne 0 ]; then
		echo Failed!
		exit 1
	fi

	umount -f /mnt/usb > /dev/null 2>&1

	exit 0
}

screenres_description="Configures screen resolution used by the user interface"
screenres_usage="
Usage:\t${bldwht}em screenres <profile>${txtrst} (where <profile> is either 'auto' or 'eonon')\n
\tex: em screenres eonon"
screenres_start() {
	if [ ${#} -ne 1 ]; then
		echo -e ${screenres_usage}
		exit 1
	fi

	echo -ne "	${STAR} Setting screen resolution ... "
	rm -f /var/em/xorg-monitor.conf
	if [ "${1}" = "auto" ]; then
		echo -e ${OK}
	elif [ "${1}" = "eonon" ]; then
		ln -s /etc/X11/monitor-eonon.conf /var/em/xorg-monitor.conf
		echo -e ${OK}
	else
		echo -e ${screenres_usage}
		exit 1
	fi

	echo -e "\nSwitch back to the UI (usually CTRL+ALT+F2), then hit CTRL+ALT+BACKSPACE to restart the graphics system"
}
