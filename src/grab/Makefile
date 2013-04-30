COMPILER = g++
CCFLAGS = -O2 -Wall -fomit-frame-pointer -DVMS_GRAB
PROGRAM = /usr/local/apps/vms_recording/bin/vms-grab

all:
	rm -f ${PROGRAM}
	${COMPILER} ${CCFLAGS} grab.cpp ../config.cpp ../util.cpp bmp.cpp v4l2.cpp /usr/lib64/libjpeg.so -o ${PROGRAM}

clean: 
	rm -f *.o

