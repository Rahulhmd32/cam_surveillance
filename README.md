# Cam Surveillance

Minimal DMP Di1 camera-capture application and GitHub Actions build.

## Run on Di1

```sh
./cam_surveillance_di1 /dev/video3 640 480
```

The application captures raw YUYV frames. Add cloud upload logic where indicated
in `src/main.cpp`; the driver-owned frame must be consumed or copied before
`releaseFrame()` is called.

## CI artifact

Pushes and pull requests targeting `main` or `master` run the Di1 cross-build.
The workflow uploads `cam-surveillance-di1-<commit-sha>` as a GitHub Actions
artifact retained for 30 days.
