---
id: developing
title: Developing
---

# Developing

You need:

- Docker
- A hacked switch

Open [Sphaira](https://github.com/ITotalJustice/sphaira). I use it over hbmenu because I don't have to keep pressing buttons.

Then run:

```bash
make deploy SWITCH_IP=xxx.xx.xx.xx
```

`nxlink` sends the build straight to your switch.

`make build` only builds the NRO. The builder image is tagged with a hash of the `Dockerfile`, `scripts/docker` and the uam submodule, so changing any of them builds a fresh image on the next run. Builds are incremental, and `make clean` starts over.

## Testing patches to the libraries

Every library akira builds against can be swapped for a local copy for one build, without moving a submodule or cutting a release:

| Variable | Points at |
| --- | --- |
| `WITH_CORE` | a libchiaki-core checkout |
| `WITH_BOREALIS` | a borealis checkout |
| `WITH_WIREGUARD` | a switch-wireguard checkout |
| `WITH_LWIP` | an lwIP checkout |
| `WITH_UAM` | a uam checkout, rebuilt inside the container |
| `WITH_NX_DEPS` | a native-deps pool, `out/aarch64/pool`, replacing the release |
| `WITH_CURL`, `WITH_FFMPEG`, `WITH_NGHTTP2`, `WITH_SIMDUTF` | one installed library prefix |

```bash
make build WITH_CORE=~/projects/libchiaki-core WITH_CURL=~/projects/curl/out
```

The overrides are mounted read-only. Changing the set of prebuilt libraries or uam starts a clean build so nothing stale gets linked.

## Pulling crash logs

```bash
make crash SWITCH_IP=xxx.xx.xx.xx
```

```console
===>    Using FTP Config: ftp://192.168.20.2:5000
===>    Requesting crash report list from ftp://192.168.20.2:5000/atmosphere/crash_reports
===>    Requesting latest report: ftp://192.168.20.2:5000/atmosphere/crash_reports/01766852145_01001f5010dfa000.log
===>    Exception Info:
    Type:                        Data Abort
    Address:                     000000aaa9fce6d0
    Fault Address:               0000000000000000
===>    LR:
0x0000000000ec673c: _malloc_r at ??:?
===>    PC:
0x0000000000ec6b10: _malloc_r at ??:?
===>    Stack info:
        ReturnAddress[00]:       0000002ee9687798 (akira + 0xedd798)
        ReturnAddress[01]:       0000002ee960a0bc (akira + 0xe600bc)
        ReturnAddress[02]:       0000002ee88b6c88 (akira + 0x10cc88)
        ReturnAddress[03]:       0000002ee88b6d78 (akira + 0x10cd78)
        ReturnAddress[04]:       0000002ee88b6de0 (akira + 0x10cde0)
        ReturnAddress[05]:       0000002ee88b8a24 (akira + 0x10ea24)
        ReturnAddress[06]:       0000002ee88b9ea8 (akira + 0x10fea8)
...
```
