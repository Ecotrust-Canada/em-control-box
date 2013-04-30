NAME="monitor"
monitor_description="Starts serial monitor to listen to device output"
monitor_usage="
Usage:\t${bldwht}em monitor <device>\t\t${txtrst}(where <device> is 'gps' or 'rfid' or 'arduino')\n
\tex: em monitor gps"

monitor_start() {
	case "${1}" in
		"gps")
			/usr/bin/gpsmon
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
