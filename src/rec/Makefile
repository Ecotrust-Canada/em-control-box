TARGET := ../../bin/em-rec
SRCS := config.cpp StateMachine.cpp Sensor.cpp GPSSensor.cpp ADSensor.cpp RFIDSensor.cpp md5.cpp em-rec.cpp
OBJS := ${SRCS:.cpp=.o} 
DEPS := ${SRCS:.cpp=.dep} 
XDEPS := $(wildcard ${DEPS}) 

CC = g++
CCFLAGS = -O2 -march=atom -fomit-frame-pointer -DEM_REC -Wall -Werror
LDFLAGS =
LIBS = -lgps -lblkid

.PHONY: all pre-build clean
all:: pre-build ${TARGET} 

ifneq (${XDEPS},) 
include ${XDEPS} 
endif 

${TARGET}: ${OBJS}
	${CC} ${LDFLAGS} -o $@ $^ ${LIBS} 

${OBJS}: %.o: %.cpp %.dep 
	${CC} ${CCFLAGS} -o $@ -c $< 

${DEPS}: %.dep: %.cpp Makefile 
	${CC} ${CCFLAGS} -MM $< > $@ 

pre-build::
	/opt/em/src/rec/make_states_header.sh

clean:: 
	rm -f *~ *.o states.h ${TARGET} 
