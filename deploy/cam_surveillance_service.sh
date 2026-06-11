#!/bin/sh

PID=/var/run/cam_surveillance.pid
RUN=/media/diskc/cam_surveillance/run.sh

case "$1" in
  start)
    start-stop-daemon -S -b -m -p "$PID" -x "$RUN"
    ;;
  stop)
    start-stop-daemon -K -p "$PID"
    rm -f "$PID"
    ;;
  restart)
    "$0" stop
    sleep 1
    "$0" start
    ;;
  status)
    if [ -f "$PID" ] && kill -0 "$(cat "$PID")" 2>/dev/null; then
      echo "running pid=$(cat "$PID")"
    else
      echo stopped
      exit 1
    fi
    ;;
  *)
    echo "usage: $0 {start|stop|restart|status}"
    exit 1
    ;;
esac
