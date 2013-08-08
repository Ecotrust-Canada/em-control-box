#!/bin/sh
MAJOR=`grep fanout /proc/devices | awk '{print $1}'`
/bin/mknod /dev/cam0 c $MAJOR 0
/bin/mknod /dev/cam1 c $MAJOR 1

