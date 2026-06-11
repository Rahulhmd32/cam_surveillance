# Cam Surveillance

DMP Di1 `/dev/video3` surveillance streamer for the Sparsh VMS dashboard.

```sh
./cam_surveillance_di1 /dev/video3 dmp-di1-surveillance-01 dmp-di1-devkit-01 148.113.39.217
```

It captures YUYV frames, JPEG-encodes them, registers as a `surveillance`
device, sends heartbeats, and streams the freshest frame over WebSocket.
