TARGET := ../../bin/em-rec
SRCS := output.cpp config.cpp StateMachine.cpp Sensor.cpp GPSSensor.cpp ADSensor.cpp RFIDSensor.cpp liveRTSP.cpp CaptureManager.cpp md5.cpp em-rec.cpp
OBJS := $(SRCS:.cpp=.o) 
DEPS := $(SRCS:.cpp=.dep) 
XDEPS := $(wildcard $(DEPS))

CC = g++
CCOPTS = -std=c++11 -Wall -Werror -pthread -DEM_REC 
OPTFLAGS = -O2 -march=atom -fomit-frame-pointer
DEBUGFLAGS = -O0 -march=atom -g -DDEBUG

CCFLAGS = $(CCOPTS) $(OPTFLAGS)

LIBS = -lpthread -lblkid -lgps -lliveMedia -lBasicUsageEnvironment -lUsageEnvironment -lgroupsock

INCLUDES = -I/usr/include/liveMedia/ -I/usr/include/UsageEnvironment -I/usr/include/groupsock

.PHONY: all debug pre-build clean
all:: pre-build $(TARGET)

debug: CCFLAGS = $(CCOPTS) $(DEBUGFLAGS)
debug: all

ifneq ($(XDEPS),) 
include $(XDEPS) 
endif 

$(TARGET): $(OBJS)
	$(CC) $(CCFLAGS) -o $@ $^ $(LIBS)

$(OBJS): %.o: %.cpp %.dep 
	$(CC) $(CCFLAGS) $(INCLUDES) -o $@ -c $< 

$(DEPS): %.dep: %.cpp
	$(CC) $(CCFLAGS) -MM $< > $@ 

pre-build::
	/opt/em/src/rec/make_states_header.sh

clean:: 
	rm -f *~ *.o states.h $(TARGET) 
