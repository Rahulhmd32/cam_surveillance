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
