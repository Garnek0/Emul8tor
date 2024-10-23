# Emul8tor

Emul8tor is an emulator for the CHIP-8 architecture.

The goal of this project is to teach myself (and others, perhaps,) about the basics of emulation.

## Features

Besides the beeper (which is not yet implemented), the emulator is pretty accurate. I've tested it with about 15 ROMs, including an 8 ROM test suite, and they all worked fine.

## Building and Running

### Linux
In the root directory, run the following commands:
```
mkdir build
cd build
cmake ..
make
./emul8tor path/to/your/rom.ch8
```

### Windows
I dunno... I haven't tested it, but the process should be similar to how you'd do it on Linux. If you have WSL then you can simply build and run it there.
