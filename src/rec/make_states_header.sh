#!/bin/sh

HEADER="states.h"

echo "Making states files ..."

# python inline
#echo -e "`/usr/bin/python - <<END
echo "`/usr/bin/python - <<END
import json
states = json.loads(open("../../public/states.json","r").read())
states.update(json.loads(open("../../public/options.json","r").read()))
for k,v in states.items():
    print "#define %s %s" % (k, v['flag'])

END`" > new${HEADER}

if ! `diff -q ${HEADER} new${HEADER} > /dev/null 2>&1`; then
	mv new${HEADER} ${HEADER}
else
	rm -f new${HEADER}
fi
