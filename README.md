# webrtc-client

A test messaging client using WebRTC written in C and [libdatachannel](https://github.com/paullouisageneau/libdatachannel) (POSIX only because Windows does not have `<pthread.h>` or `<uuid/uuid.h>`, TODO: use Cygwin or cross-platform alternative libraries)

Requires a signalling server, [rustysignal](https://github.com/liraymond04/rustysignal) is the one I used during testing

## Building

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
