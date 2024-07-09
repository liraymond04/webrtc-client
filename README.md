# webrtc-client

A native client library for WebRTC written in C and [libdatachannel](https://github.com/paullouisageneau/libdatachannel) (POSIX only because Windows does not have `<pthread.h>` or `<uuid/uuid.h>`, TODO: use Cygwin or cross-platform alternative libraries)

Requires a signalling server, [rustysignal](https://github.com/liraymond04/rustysignal) is the one I used during testing, and includes custom protocol for rooms used in the `chat` example

## Examples

- `chat`: TUI chat application using ncurses, use the `help` command to see what you can do!
- `game`: Simple GUI "game" using [olc PGE](https://github.com/Moros1138/olcPixelGameEngineC), see your friends shmovin' in real-time (or 60fps, give or take). Use the WASD keys to move around, and use Esc to exit the game.

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

And run one of the example applications

```bash
# Look in the examples folder to see what you can try,
# the executables should have the same names as their
# respective source files
$ ./build/chat
```
