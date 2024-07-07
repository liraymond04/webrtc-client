# webrtc-client

A native client library for WebRTC written in C and [libdatachannel](https://github.com/paullouisageneau/libdatachannel) (POSIX only because Windows does not have `<pthread.h>` or `<uuid/uuid.h>`, TODO: use Cygwin or cross-platform alternative libraries)

Requires a signalling server, [rustysignal](https://github.com/liraymond04/rustysignal) is the one I used during testing, and includes custom protocol for rooms used in the `chat` example

## Building

Prerequesites are `cmake`, `ninja`, and `gcc`, which you can install with your distribution package manager

Ex) Arch Linux

```bash
$ sudo pacman -S cmake ninja gcc
```

Generate CMake build files and start build

```bash
# You can pass in -DBUILD_EXAMPLES=OFF to only build the library
# and -DBUILD_STATIC_LIBS=OFF to build as shared library
$ cmake -B build -G Ninja
$ cmake --build build
```

And run the example chat application

```bash
$ ./build/chat
```
