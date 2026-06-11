#!/bin/sh

export LD_LIBRARY_PATH=/usr/lib

exec /media/diskc/cam_surveillance/cam_surveillance_di1 \
  /dev/video3 \
  dmp-di1-surveillance-01 \
  dmp-di1-devkit-01 \
  148.113.39.217 >> /media/diskc/cam_surveillance/cam_surveillance.log 2>&1
