# Cam Surveillance

DMP Di1 `/dev/video3` surveillance streamer for the Sparsh VMS dashboard.

```sh
./cam_surveillance_di1 /dev/video3 dmp-di1-surveillance-01 dmp-di1-devkit-01 148.113.39.217
```

It captures YUYV frames, JPEG-encodes them, registers as a `surveillance`
device, sends heartbeats, and streams the freshest frame over WebSocket.

## Di1 build

The build uses the same vendor iCatch/OpenWrt glibc toolchain as the DMS Di1
build:

```sh
export PATH="/opt/di1-sdk/toolchain/aarch64-openwrt-icatchtek-bsp4-glibc/toolchain-aarch64_cortex-a53_gcc-7.3.0_glibc/bin:$PATH"
make
```

GitHub Actions downloads the pinned toolchain bundle from the private
`josegibson/DMS_sparshiq` `toolchain-v1` release. Configure the
`DI1_TOOLCHAIN_TOKEN` repository secret with read-only access to that
repository.

## Di1 device setup (one-time)

The device's `/usr/lib/libicatcv.so` (BSP 11.4.1) is missing `iCatCvBufCopy`,
which `libdi1utils.so` calls at runtime. Copy the correct version from the
railtrack repo backups:

```sh
cat railtrack-od-dmp-di1/backups/last_success/libicatcv.so \
  | ssh root@192.168.100.60 \
    "mkdir -p /media/diskc/cam_surveillance/lib && \
     dd of=/media/diskc/cam_surveillance/lib/libicatcv.so"
```

This only needs to be done once — `/media/diskc` survives reboots.

## Di1 deploy

Download the artifact from GitHub Actions and copy to the device (no scp/sftp
— the device runs BusyBox dropbear):

```sh
cat cam_surveillance_di1 | ssh root@192.168.100.60 \
  "dd of=/media/diskc/cam_surveillance/cam_surveillance_di1 && \
   chmod +x /media/diskc/cam_surveillance/cam_surveillance_di1"

cat run_cam_surveillance.sh | ssh root@192.168.100.60 \
  "dd of=/media/diskc/cam_surveillance/run.sh && \
   chmod +x /media/diskc/cam_surveillance/run.sh"

cat cam_surveillance_service.sh | ssh root@192.168.100.60 \
  "dd of=/media/diskc/cam_surveillance/service.sh && \
   chmod +x /media/diskc/cam_surveillance/service.sh"
```

Then restart:

```sh
ssh root@192.168.100.60 "/media/diskc/cam_surveillance/service.sh restart"
```

## Runtime notes

- The launcher (`run.sh`) loads the NPU kernel modules (`dmpvgkm.ko`,
  `dmpsv.ko`, `a3000.ko`) if not already loaded. These are required even
  though cam_surveillance does no inference — `libdi1utils.so` and `libOpenVG`
  depend on them.
- `LD_LIBRARY_PATH=/media/diskc/cam_surveillance/lib:/usr/lib` ensures the
  patched `libicatcv.so` is picked up before the system one.
- The service is started on boot via `/media/diskc/autorun.sh`.
- Logs: `/media/diskc/cam_surveillance/cam_surveillance.log`
