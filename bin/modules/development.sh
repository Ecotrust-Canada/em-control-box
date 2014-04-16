NAME="buildard buildem installem resetgps2 stripnodeapp syncroot"

buildard_description="Builds Arduino analog data collector images"
buildard_start() {
	unset CFLAGS
	unset CXXFLAGS

	for BOARD in ${BOARDS}; do
		echo -e "	${STAR} Building Arduino software (em-adc.pde) for ${BOARD} ... " &&
		rm -rf /tmp/arduino && mkdir /tmp/arduino &&
		cp /opt/em/arduino/em-adc.pde /tmp/arduino/ &&
		echo "BOARD_TAG = ${BOARD}" >> /tmp/arduino/Makefile &&
		echo "ARDUINO_DIR = /opt/arduino" >> /tmp/arduino/Makefile &&
		echo "ARDMK_DIR = /opt/arduino" >> /tmp/arduino/Makefile &&
		echo "AVR_TOOLS_DIR = /opt/arduino/avr-tools" >> /tmp/arduino/Makefile &&
		echo "AVRDUDE = /usr/bin/avrdude" >> /tmp/arduino/Makefile &&
		echo "AVRDUDE_CONF = /etc/avrdude.conf" >> /tmp/arduino/Makefile &&
		echo "CFLAGS = " >> /tmp/arduino/Makefile &&
		echo "CXXFLAGS = " >> /tmp/arduino/Makefile &&
		echo "include /opt/arduino/Arduino.mk" >> /tmp/arduino/Makefile &&
		cd /tmp/arduino &&
		make > /dev/null 2>&1 &&
		cp /tmp/arduino/build-${BOARD}/arduino.hex /opt/em/arduino/${BOARD}.hex
	done

	rm -rf /tmp/arduino
}

installem_description="Installs EM software on a blank OS disk"
installem_usage="
Usage:\t${bldwht}em installem <device> <version>\t\t${txtrst}(where <device> is the OS SSD, <version> is an EM release)\n
\tex: em installem /dev/sdb 2.0.0"
installem_start() {
	if [ ${#} -ne 2 ]; then
		ls -l /dev/sd*
		ls -l /opt/em/images/em-*
		echo
		echo -e ${installem_usage}
		exit 1
	fi

	if [ ${UID} -ne 0 ]; then
		echo -e "${bldred}You must be root to use this command${txtrst}"
		exit 1
	fi

	IMAGE="/opt/em/images/em-${2}"
	if [ ! -f ${IMAGE} ]; then
		echo -e "${bldred}Image ${bldwht}${IMAGE}${bldred} can't be found${txtrst}"
		exit 1
	fi

	RELEASE=${2}
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
	echo -e "2048,4194304,L,*\n4196352,33554432,L\n37750784,,L" | sfdisk -uS -qL ${DEVICE} > /dev/null 2>&1 &&
	echo -e ${OK}
	
	echo -ne "	${STAR} Formatting /boot ... " &&
	mkfs.ext4 -q -L BOOT -O none,dir_index,extent,filetype,flex_bg,has_journal,sparse_super,uninit_bg,large_file -E discard -b 4096 -I 256 -i 65536 -m 1 ${DEVICE}1 &&
	echo -e ${OK}

	echo -ne "	${STAR} Formatting /var ... " &&
	mkfs.ext4 -q -L VAR -O none,dir_index,extent,filetype,flex_bg,has_journal,sparse_super,uninit_bg,large_file -E discard -b 4096 -I 256 -i 16384 -m 1 ${DEVICE}2 &&
	echo -e ${OK}

	echo -ne "	${STAR} Formatting DEV partition ... " &&
	mkfs.ext4 -q -L DEV -O none,dir_index,extent,filetype,flex_bg,has_journal,sparse_super,uninit_bg,large_file -E discard -b 4096 -I 256 -m 1 ${DEVICE}3 &&
	echo -e ${OK}

	echo -ne "	${STAR} Mounting new partitions at /mnt/install ... " &&
	umount -f /mnt/install > /dev/null 2>&1
	rm -rf /mnt/install
	mkdir /mnt/install
	mount ${DEVICE}3 /mnt/install
	mkdir -p /mnt/install/boot /mnt/install/var
	mount ${DEVICE}1 /mnt/install/boot
	mount ${DEVICE}2 /mnt/install/var
	echo -e ${OK}

	echo -ne "	${STAR} Creating skeleton structure and swap file ... " &&
	cd /mnt/install
	mkdir -p boot/grub var/cache/fontconfig var/cache/ldconfig var/em/data/archived var/em/data/reports var/em/data/screenshots var/em/data/video var/elog var/lib/dbus var/lib/hwclock var/lib/sshd var/lib/systemd/catalog var/lib/xkb var/log/journal
	chmod 750 var/cache/ldconfig
	ln -s /run/lock var/lock
	ln -s /run var/run
	ln -s /tmp var/tmp
	dd if=/dev/zero of=/mnt/install/var/swapfile bs=256K count=2048 > /dev/null 2>&1
	chmod 0600 /mnt/install/var/swapfile
	mkswap /mnt/install/var/swapfile > /dev/null
	cp /opt/em/src/em.conf /opt/em/src/encoding.conf /mnt/install/var/em/

	dbus-uuidgen --ensure=/mnt/install/var/lib/dbus/machine-id
	journalctl --update-catalog
	cp --preserve=all /var/lib/systemd/catalog/database /mnt/install/var/lib/systemd/catalog/
	#chown -R ecotrust:ecotrust var/em
	echo -e ${OK}

	echo -ne "	${STAR} Copying dev partition files (takes a while) ... " &&
	echo -n ${RELEASE} > /mnt/install/em-release
	cp -av /boot/vmlinuz* /mnt/install/boot/
	cd /mnt/install && mkdir -p dev mnt/data proc root run sys tmp
	cd / && cp -avP bin etc home lib lib64 opt sbin usr /mnt/install/
	echo ${OK}

	echo -ne "	${STAR} Installing ${IMAGE} ... " &&
	cp ${IMAGE} /mnt/install/boot/
	echo -e ${OK}

	echo -ne "	${STAR} Installing GRUB ... " &&
	grub-install --boot-directory=/mnt/install/boot ${DEVICE} > /dev/null
	cp /opt/em/src/grub.cfg /mnt/install/boot/grub/
	echo "menuentry 'EM ${RELEASE}' --class electronic --class gnu-linux --class gnu --class os --unrestricted \$menuentry_id_option 'em-${RELEASE}' {" >> /mnt/install/boot/grub/grub.cfg
	echo "savedefault" >> /mnt/install/boot/grub/grub.cfg
	echo "insmod gzio" >> /mnt/install/boot/grub/grub.cfg
	echo "insmod part_msdos" >> /mnt/install/boot/grub/grub.cfg
	echo "insmod ext2" >> /mnt/install/boot/grub/grub.cfg
	echo "set root=(hd0,msdos1)" >> /mnt/install/boot/grub/grub.cfg
	echo "echo 'Loading EM software v${RELEASE} ...'" >> /mnt/install/boot/grub/grub.cfg
	echo "linux /em-${RELEASE} ro quiet" >> /mnt/install/boot/grub/grub.cfg
	echo "}" >> /mnt/install/boot/grub/grub.cfg
	echo >> /mnt/install/boot/grub/grub.cfg
	echo -e ${OK}

	echo -ne "	${STAR} Editing /mnt/install/var/em/em.conf ... " &&
	vim /mnt/install/var/em/em.conf
	echo -e ${OK}

	echo -ne "	${STAR} Unmounting /mnt/install/boot and /mnt/install/var ... " &&
	sync
	umount /mnt/install/var
	umount /mnt/install/boot
	umount /mnt/install
	echo -e ${OK}

	echo
	echo -e "${txtgrn}Finished!"
}

buildem_description="Builds EM image file"
buildem_usage="
Usage:\t${bldwht}em buildem <release version> <kernel version>${txtrst}\n
\tex: em buildem 2.2.0 3.11.4\n\n
\t/usr/src/linux-3.11.4 has to exist"
buildem_start() {
    if [ ${#} -ne 2 ]; then
            echo
            echo -e ${buildem_usage}
            exit 1
    fi

    if [ ${UID} -ne 0 ]; then
            echo -e "${bldred}You must be root to use this command${txtrst}"
            exit 1
    fi

	VER=${1}
	DEST=/usr/src/em-${1}
	KERNEL_VERSION=${2}

	if [ "`uname -r`" != "${KERNEL_VERSION}-em" ]; then
		echo -e "${bldred}You must be running the kernel against which you wish to build${txtrst}"
		exit 1
	fi

    mozplugger-update

	echo -ne "	${STAR} Building em-rec ... " &&
	cd /opt/em/src/rec && make clean
	./make_states_header.sh && make -j4 > /dev/null
	echo -e ${OK}
 
	buildard_start

	echo -ne "	${STAR} Preparing image root at ${DEST} ... " &&
	rm -rf ${DEST} && mkdir -p ${DEST}
	cd ${DEST}
	echo -n ${VER} > ${DEST}/em-release
	mkdir -p boot dev mnt/data proc root run sys tmp var
	echo -e ${OK}

	echo -ne "	${STAR} Copying files ... " &&
	for FILE in `cat /opt/em/src/files.lst`; do cp -r --parents --no-dereference --preserve=all /$FILE ${DEST}/; done
	cp /opt/em/src/fstab /opt/em/src/resolv.conf ${DEST}/etc/
	ln -s /sbin/init ${DEST}/init
	echo -e ${OK}

	stripnodeapp_start /opt/elog ${DEST}/opt/elog
	stripnodeapp_start /opt/em ${DEST}/opt/em

	echo -ne "	${STAR} Stripping executables ... " &&
	find ${DEST} | xargs file | grep "executable" | grep ELF | cut -f 1 -d : | xargs -r strip --strip-unneeded
	echo -e ${OK}

	echo -ne "	${STAR} Stripping libraries ... " &&
	find ${DEST} | xargs file | grep "shared object" | grep ELF | cut -f 1 -d : | xargs -r strip --strip-unneeded
	echo -e ${OK}

	echo -ne "	${STAR} Stripping other archives ... " &&
	find ${DEST} | xargs file | grep "current ar archive" | cut -f 1 -d : | xargs -r strip --strip-unneeded
	echo -e ${OK}

	echo -ne "	${STAR} Running ldconfig ... " &&
	ldconfig -r ${DEST}
	echo -e ${OK}

	echo -e "	${STAR} Building Linux kernel, modules, and updating ${DEST} ... " && 
	cp /opt/em/src/config-${VER}-stage1 /usr/src/linux-${KERNEL_VERSION}/.config
	rm -f /usr/src/linux && ln -s /usr/src/linux-${KERNEL_VERSION} /usr/src/linux
	cd /usr/src/linux-${KERNEL_VERSION}
	rm -f usr/initramfs_data.cpio*
	make -j4 bzImage > /dev/null
	make -j4 modules > /dev/null && make modules_install > /dev/null
	make -C /usr/src/linux-${KERNEL_VERSION} M=/opt/em/src clean
	make -C /usr/src/linux-${KERNEL_VERSION} M=/opt/em/src modules
	mkdir -p /lib/modules/${KERNEL_VERSION}-em/kernel/drivers/char
	cp /opt/em/src/fanout.ko /lib/modules/${KERNEL_VERSION}-em/kernel/drivers/char/

	cd /opt/em/src/e1000e-3.0.4.1/src
	make clean > /dev/null 2>&1
	make BUILD_KERNEL=${KERNEL_VERSION} CFLAGS_EXTRA=-DDISABLE_PCI_MSI install > /dev/null 2>&1

	depmod -a

	cp -r --parents --no-dereference --preserve=all /lib/modules/${KERNEL_VERSION}-em ${DEST}/
	echo -e ${OK}

	echo -e "	${STAR} Creating initramfs CPIO archive ... " &&
	rm -f /usr/src/initramfs_data.cpio.gz
	cd ${DEST}
	find . | cpio -o -H newc | gzip -9 > /usr/src/initramfs_data.cpio.gz
	echo -e ${OK}

	echo -e "	${STAR} Rebuilding Linux kernel w/ initramfs from ${DEST} ... " &&
	cp /opt/em/src/config-${VER}-stage2 /usr/src/linux-${KERNEL_VERSION}/.config
	cd /usr/src/linux-${KERNEL_VERSION}
	rm -f usr/initramfs_data.cpio*
	make -j2 bzImage > /dev/null
	cp arch/x86/boot/bzImage /opt/em/images/em-${VER}
	echo -e ${OK}

	echo
	echo -e Your image: ${bldwht}/opt/em/images/em-${VER}${txtrst}
}

resetgps2_description="Resets Garmin GPS to GPRMC-only configuration"
resetgps2_start() {
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

	echo -ne "	${STAR} Enabling GPRMC ... "
	echo -ne '$PGRMO,GPRMC,1*3D\r\n' > ${GPS_DEV}
	sleep 2
	echo -e ${OK}

	echo -ne "	${STAR} Resetting GPS ... "
	echo -ne '$PGRMI,,,,,,,R\r\n' > ${GPS_DEV}
	echo -e ${OK}
}

stripnodeapp_description="Copies node.js app to new destination without useless files"
stripnodeapp_usage="
Usage:\t${bldwht}em stripnodeapp <path/to/app> <destination>${txtrst}\n
\tex: em stripnodeapp /opt/elog /mnt/install/opt/elog"
stripnodeapp_start() {
    if [ ${#} -ne 2 ]; then
            echo
            echo -e ${stripnodeapp_usage}
            exit 1
    fi
 
	STARTDIR=`pwd`
	APP=${1}
	NEWAPP=${2}

	echo -e "	${STAR} Creating clean copy of ${APP} in ${NEWAPP} ... " &&
	mkdir -p ${NEWAPP} &&
	cp -a --no-dereference ${APP} ${APP}.copy &&
	cd ${APP}.copy
	find . -name test -type d | xargs rm -rf
	find . -name tests -type d | xargs rm -rf
	find . -name testing -type d | xargs rm -rf
	find . -name example -type d | xargs rm -rf
	find . -name examples -type d | xargs rm -rf
	find . -name "doc*" -type d | xargs rm -rf
	find . -name ".git" | xargs rm -rf

	find . -name "*.coffee" | xargs coffee -c
	find . -name "*.styl" | xargs stylus -c

	rsync -a --include "*/" --include "*.js" --exclude "*" ${APP}.copy/ ${NEWAPP}/
	rsync -a --include "*/" --include "*.json" --exclude "*" ${APP}.copy/ ${NEWAPP}/
	rsync -a --include "*/" --include "*.jsm" --exclude "*" ${APP}.copy/ ${NEWAPP}/
	rsync -a --include "*/" --include "*.map" --exclude "*" ${APP}.copy/ ${NEWAPP}/
	rsync -a --include "*/" --include "*.jade" --exclude "*" ${APP}.copy/ ${NEWAPP}/
	rsync -a --include "*/" --include "*.css" --exclude "*" ${APP}.copy/ ${NEWAPP}/
	rsync -a --include "*/" --include "*.types" --exclude "*" ${APP}.copy/ ${NEWAPP}/

	KEEP="`find . -name "*bin" -type d`"
	for F in ${KEEP}
	do
	    cp -an --parents --no-dereference ${F} ${NEWAPP}/
	done

	cd ${STARTDIR}
	rm -rf ${APP}.copy
	echo -e ${OK}
}

syncroot_description="Syncs / dirs (including opt) between networked dev EM boxes w/ dry run; deletes files on target!"
syncroot_usage="
Usage:\t${bldwht}em syncroot <from/to> <IP>${txtrst}\n
\tex: em syncroot to 10.10.40.87\n
\tex: em syncroot from 1.1.1.20"
syncroot_start() {
	if [ ${#} -ne 2 ]; then
		echo
		echo -e ${syncroot_usage}
		exit 1
	fi

	if [ "${1}" == "from" ]; then
		SOURCE="root@${2}:/"
		TARGET="/"
	elif [ "${1}" == "to" ]; then
		SOURCE="/"
		TARGET="root@${2}:/"
	else
		echo
		echo -e ${syncroot_usage}
		exit 1
	fi

	RSYNC_COMMAND="/usr/bin/rsync -avxS --delete ${SOURCE} ${TARGET} --exclude=etc/conf.d/network"

	DELETED=`echo -e ${bldred}`
	NORMAL=`echo -e ${txtrst}`

	echo -e "	${STAR} Doing a dry run first ... " &&
	${RSYNC_COMMAND} -n | sed -e "s/^deleting.*$/${DELETED}&${NORMAL}/"
	echo

	read -p "Are you sure after reviewing dry run? (y/n) " yn
	case $yn in
		[Yy]* )
			echo -e "	${STAR} Syncing / dirs for real now ... " &&
			${RSYNC_COMMAND} &&
			echo -e ${OK}
			;;
		[Nn]* )
			exit
			;;
		* )
			echo "(y/n)"
			;;
	esac
}

