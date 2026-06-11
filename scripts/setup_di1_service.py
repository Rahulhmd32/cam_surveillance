#!/usr/bin/env python3

import argparse
import time

import serial


RUN_SCRIPT = """#!/bin/sh
export LD_LIBRARY_PATH=/usr/lib
exec /media/diskc/cam_surveillance/cam_surveillance_di1 \
  /dev/video3 \
  dmp-di1-surveillance-01 \
  dmp-di1-devkit-01 \
  148.113.39.217 >> /media/diskc/cam_surveillance/cam_surveillance.log 2>&1
"""

SERVICE_SCRIPT = """#!/bin/sh
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
"""


def send_command(port: serial.Serial, command: str, wait_s: float = 0.2) -> None:
    port.write((command + "\n").encode())
    port.flush()
    time.sleep(wait_s)
    port.read(65536)


def write_file(port: serial.Serial, path: str, content: str) -> None:
    send_command(port, f"rm -f {path}")
    quote = "'"
    data = content.encode()
    for offset in range(0, len(data), 128):
        escaped = "".join(f"\\x{byte:02x}" for byte in data[offset : offset + 128])
        send_command(port, f"printf {quote}{escaped}{quote} >> {path}", 0.05)


def read_until(port: serial.Serial, marker: bytes, timeout_s: float) -> bytes:
    end = time.time() + timeout_s
    output = b""
    while time.time() < end:
        output += port.read(8192)
        if marker in output:
            break
    return output


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--port", default="/dev/cu.usbserial-1101")
    args = parser.parse_args()

    with serial.Serial(args.port, 115200, timeout=0.1, write_timeout=10) as port:
        port.reset_input_buffer()
        write_file(port, "/media/diskc/cam_surveillance/run.sh", RUN_SCRIPT)
        write_file(port, "/media/diskc/cam_surveillance/service.sh", SERVICE_SCRIPT)
        command = (
            "chmod +x /media/diskc/cam_surveillance/run.sh "
            "/media/diskc/cam_surveillance/service.sh; "
            "grep -q cam_surveillance/service.sh /media/diskc/autorun.sh || "
            "printf '\\n/media/diskc/cam_surveillance/service.sh start\\n' "
            ">> /media/diskc/autorun.sh; "
            "sync; "
            "/media/diskc/cam_surveillance/service.sh restart; "
            "sleep 3; "
            "/media/diskc/cam_surveillance/service.sh status; "
            "tail -20 /media/diskc/cam_surveillance/cam_surveillance.log; "
            "echo __SETUP_DONE__"
        )
        port.write((command + "\n").encode())
        print(read_until(port, b"__SETUP_DONE__", 20).decode("utf-8", "replace"))


if __name__ == "__main__":
    main()
