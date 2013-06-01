NAME="clear format mount unmount havedatadisk freespace"
clear_description="Clears the data disk"
format_description="Partitions, formats, labels, and prepares a new data disk"
format_usage="
Usage:\t${bldwht}em format\t\t\t${txtrst}(Easy prompts for device and EM disk number)\n
\t${bldwht}em format <device> <emdisk#>\t${txtrst}(No prompts, expert use)\n
\t\tex: em format sdb 154"
mount_description="Mounts the data disk"
unmount_description="Unmounts the data disk"
havedatadisk_description="Checks if there is a data disk mounted and usable"
freespace_description="Shows free space available on data disk (or OS disk if no data disk)"
setperms_description="Sets permissions/ownership on key EM files"

check_var_safety() {
	if [ -z "${DATA_MNT}" ]; then
		echo -e "${bldred}DATA_MNT not properly set in em.conf${txtrst}"
		exit 1
	fi

	if [ -z "${BACKUP_DATA_MNT}" ]; then
		echo -e "${bldred}BACKUP_DATA_MNT not properly set in em.conf${txtrst}"
		exit 1
	fi

	if ! havedatadisk_start > /dev/null 2>&1; then
		echo -e "${bldred}No data disk mounted at ${DATA_MNT}${txtrst}"
		exit 1
	fi
}

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

		echo -ne "	${STAR} Clearing ELOG header ... " &&
		echo "{}" > ${ELOG_HEADER} &&
		echo -e ${OK}

		echo -ne "	${STAR} Clearing local RFID database ... " &&
		echo "{}" > ${SCANNED_TAGS} &&
		echo -e ${OK}

		echo -ne "	${STAR} Clearing resolution cache ... " &&
		sudo -u user rm ${EM_RESOLUTION} &&
		echo -e ${OK}

		echo -ne "	${STAR} Clearing data disk ... " &&
		rm -Rf ${DATA_MNT}/* &&
		echo -e ${OK}

		echo -ne "	${STAR} Clearing backup sensor data ... " &&
		rm -Rf ${BACKUP_DATA_MNT}/* &&
		echo -e ${OK}
	fi
}

format_start() {
	if [ ${#} -eq 0 ]; then
		while read DEV; do
			TOKENS=(${DEV})
			if [ "${TOKENS[0]}" == "sda" ]; then continue; fi

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
	sudo echo "0,,L" | sfdisk -qL ${format_DEVICE} > /dev/null 2>&1 &&
	echo -e ${OK}

	echo -ne "	${STAR} Formatting and labelling the partition ... " &&
	mkfs.ext4 -q -L ${format_LABEL} -O none,dir_index,extent,filetype,flex_bg,has_journal,sparse_super,uninit_bg,large_file -E discard -i 4096 ${format_DEVICE}1 &&
	echo -e ${OK}

	echo -ne "	${STAR} Triggering udev events ... " &&
	udevadm trigger --action=add &&
	sleep 2 &&
	echo -e ${OK}

	#mount_start
	#unmount_start
}

havedatadisk_start() {
	if (! mountpoint ${DATA_MNT} > /dev/null 2>&1) || (! sudo -u ecotrust touch ${DATA_MNT}/.writable > /dev/null 2>&1); then
		echo -n 'no'
		return 1
	else
		sudo -u ecotrust rm ${DATA_MNT}/.writable
		echo 'yes'
		LABEL="`lsblk -nro LABEL /dev/emdata 2>/dev/null`"

		if [[ ${LABEL} == DATA_* ]]; then
			LABEL="${LABEL##DATA_}"
		fi

		echo ${LABEL}
		return 0
	fi
}

freespace_start() {
	df -B1 ${BACKUP_DATA_MNT} | tail -n1 | awk '{ print $1,$4,$5 }'

	if havedatadisk_start > /dev/null 2>&1; then
		df -B1 ${DATA_MNT} | tail -n1 | awk '{ print $1,$4,$5 }'
	fi
}
