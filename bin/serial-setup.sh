#!/bin/bash
PATH=/bin:/sbin:/usr/bin:/usr/sbin

eval "`/opt/em/bin/em-rec --dump-config`"

chmod +x /tmp/user/.mozilla/plugins/mozplugger0.so

# load last good time.
fake-hwclock load

/usr/bin/setserial -z ${RFID_DEV} low_latency
/usr/bin/setserial -z ${GPS_DEV} low_latency


if [ "${fishing_area}" == "A" ]; then
	/bin/stty -F ${GPS_DEV} 4800
	/bin/stty -F ${RFID_DEV} 9600
else
	/bin/stty -F ${GPS_DEV} 38400
	/bin/stty -F ${RFID_DEV} 9600
fi



