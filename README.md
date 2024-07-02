# webrtc-client

A test messaging client using WebRTC written in C and [libdatachannel](https://github.com/paullouisageneau/libdatachannel) (POSIX only because Windows does not have `<uuid/uuid.h>`, TODO: use cross-platform uuid lib/function)

Requires a signalling server, [rustysignal](https://github.com/liraymond04/rustysignal) is the one I used during testing

## Building

Install [json-c](https://github.com/json-c/json-c) to your system following the GitHub page instructions (TODO: add to FetchContent so user is not required to install onto their system)

Prerequesites are `cmake`, `ninja`, and `gcc`, which you can install with your distribution package manager

Ex) Arch Linux

```bash
$ sudo pacman -S cmake ninja gcc
```

Generate CMake build files and start build

```bash
$ cmake -B build -G Ninja
$ cmake --build build
```

And run the client

```bash
$ ./build/webrtc-client
```
