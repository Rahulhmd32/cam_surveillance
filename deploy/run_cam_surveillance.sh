#!/bin/sh

if [ ! -e /dev/a3000-0 ]; then
    insmod /lib/modules/$(uname -r)/dmpvgkm.ko 2>/dev/null || true
    insmod /lib/modules/$(uname -r)/dmpsv.ko   2>/dev/null || true
    insmod /lib/modules/$(uname -r)/a3000.ko   2>/dev/null || true
    sleep 2
fi

export LD_LIBRARY_PATH=/media/diskc/cam_surveillance/lib:/usr/lib

exec /media/diskc/cam_surveillance/cam_surveillance_di1 \
  /dev/video3 \
  dmp-di1-surveillance-01 \
  dmp-di1-devkit-01 \
  148.113.39.217 >> /media/diskc/cam_surveillance/cam_surveillance.log 2>&1
