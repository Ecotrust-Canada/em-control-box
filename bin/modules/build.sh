NAME="ardflash buildrec installem"
ardflash_description="Builds and installs analog data collector on Arduino board"
buildrec_description="Builds em-rec and em-grab binaries (for in-lab use)"
installem_description="Installs EM software on a blank OS disk (for in-lab use)"
installem_usage="
Usage:\t${bldwht}em installem <device>\t\t${txtrst}(where <device> is the OS SSD)\n
\tex: em installem /dev/sda"

ardflash_start() {
	if [ "${arduino}" == "3.3V" -o "${arduino}" == "3.3VD" ]; then
		BOARD="pro328"
	elif [ "${arduino}" == "5V" ]; then
		BOARD="pro5v328"
	else
		echo -e "${bldred}Arduino type in /etc/em.conf not set or invalid${txtrst}" &&
		exit 1
	fi

	echo -e "	${STAR} Building Arduino software (em-adc.pde) ... " &&
	rm -rf /tmp/arduino && mkdir /tmp/arduino &&
	cp /opt/em/arduino/em-adc.pde /tmp/arduino/ &&
	echo "ARDUINO_LIBS = SoftwareSerial SD" >> /tmp/arduino/Makefile &&
	echo "BOARD_TAG = ${BOARD}" >> /tmp/arduino/Makefile &&
	echo "ARDUINO_PORT = ${ARDUINO_DEV}" >> /tmp/arduino/Makefile &&
	echo "ARDUINO_DIR = /opt/arduino" >> /tmp/arduino/Makefile &&
	echo "ARDMK_DIR = /opt/em" >> /tmp/arduino/Makefile &&
	echo "AVR_TOOLS_DIR = /opt/arduino/hardware/tools/avr" >> /tmp/arduino/Makefile &&
	echo "include /opt/arduino/Arduino.mk" >> /tmp/arduino/Makefile &&
	cd /tmp/arduino &&
	make 1>/dev/null

	echo -e "	${STAR} Flashing Arduino (${arduino}) board ... " &&
	make upload 1>/dev/null

	rm -rf /tmp/arduino
}

buildrec_start() {
	echo -ne "	${STAR} Building em-rec ... " &&
	cd /opt/em/src/rec &&
	make clean > /dev/null && make > /dev/null && make clean > /dev/null &&
	echo -e ${OK}

	echo -ne "	${STAR} Building em-grab ... " &&
	cd /opt/em/src/grab &&
	make clean > /dev/null && make > /dev/null && make clean > /dev/null &&
	echo -e ${OK}
}

installem_start() {
	if [ ${#} -ne 1 ]; then
		echo -e ${installem_usage}
		exit 1
	fi

	if [ ${UID} -ne 0 ]; then
		echo -e "${bldred}You must be root to use this command${txtrst}"
		exit 1
	fi

	DEVICE="/dev/`basename ${1}`"

	if ! lsblk ${DEVICE} > /dev/null 2>&1; then
		echo -e "${bldred}${DEVICE} is not a block device${txtrst}"
		exit 1
	fi

	echo -e "${bldred}Beginning operations on ${DEVICE} in 3 seconds ... hit CTRL+C to stop me${txtrst}"
	sleep 3

	echo -ne "	${STAR} Unmounting any mounted partitions on ${DEVICE} ... " &&
	umount -f ${DEVICE}* > /dev/null 2>&1
	echo -e ${OK}

	echo -ne "	${STAR} Clearing partition table ... " &&
	dd if=/dev/zero of=${DEVICE} bs=4096 count=1 > /dev/null 2>&1 &&
	echo -e ${OK}

	echo -ne "	${STAR} Creating new partitions ... " &&
	echo -e "2048,8388608,L,*\n8390656,8388608,L\n16779264,,L" | sfdisk -uS -qL ${DEVICE} > /dev/null 2>&1 &&
	echo -e ${OK}
	
	echo -ne "	${STAR} Formatting /boot ... " &&
	mkfs.ext4 -q -L boot -O none,dir_index,extent,filetype,flex_bg,has_journal,sparse_super,uninit_bg,large_file -E discard -i 65536 ${DEVICE}1 &&
	echo -e ${OK}

	echo -ne "	${STAR} Formatting /var ... " &&
	mkfs.ext4 -q -L var -O none,dir_index,extent,filetype,flex_bg,has_journal,sparse_super,uninit_bg,large_file -E discard -i 16384 ${DEVICE}2 &&
	echo -e ${OK}

	echo -ne "	${STAR} Formatting /home ... " &&
	mkfs.ext4 -q -L home -O none,dir_index,extent,filetype,flex_bg,has_journal,sparse_super,uninit_bg,large_file -E discard -i 4096 ${DEVICE}3 &&
	echo -e ${OK}

	# create swap: dd swapfile, mkswap, chown root:root swap, chmod 0600 swap
	# install kernel
	# install grub
	# copy current system
	# slim down current system
	# make mods to conf files (fstab, etc.)
	# build monitors
	# build squashfs image
}
