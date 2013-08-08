#!/bin/sh

STATES="../states.dat"
C_HEADER="states.h"
JSON_ARRAY="../../public/sensorStates.json"

echo "Making states files ..."
rm -f ${JSON_ARRAY}

echo -n "{" >> ${JSON_ARRAY}

i=0
while read line; do
	if [ -z "$line" ]; then continue; fi
	if [ "$i" -gt 0 ]; then echo -n , >> ${JSON_ARRAY}; fi
	parts=($line)
	echo "#define" ${parts[0]} ${parts[1]} >> new${C_HEADER}
	#echo -n \"${parts[0]}\": \{ \"flag\": \"\\u${parts[1]:2}\", \"msg\": "${parts[@]:2}" \} >> ${JSON_ARRAY}
	echo -n \"${parts[0]}\": \{ \"flag\": \"${parts[1]}\", \"msg\": "${parts[@]:2}" \} >> ${JSON_ARRAY}
	(( i++ ))
done < ${STATES}
echo "}" >> ${JSON_ARRAY}

if ! `diff -q ${C_HEADER} new${C_HEADER} > /dev/null 2>&1`; then
	mv new${C_HEADER} ${C_HEADER}
else
	rm -f new${C_HEADER}
fi
