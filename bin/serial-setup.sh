#!/bin/bash
PATH=/bin:/sbin:/usr/bin:/usr/sbin

source /opt/em/bin/read-config.func

/usr/bin/setserial -z ${GPS_DEV} low_latency
/usr/bin/setserial -z ${RFID_DEV} low_latency

if [ "${fishing_area}" == "A" ]; then
	/bin/stty -F ${GPS_DEV} 4800
	/bin/stty -F ${RFID_DEV} 9600
else
	/bin/stty -F ${GPS_DEV} 38400
	/bin/stty -F ${RFID_DEV} 9600	
fi

