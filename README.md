# Emul8tor

Emul8tor is an emulator for the CHIP-8 architecture, written in only 335 lines of code.

The goal of this project is to teach myself (and others, perhaps, but beware of the lack of comments and messy code, since another one of my goals for this emulator was to attempt to write it in as few lines of code as possible without cheating too much.) about the basics of emulation.

## Features

Besides the beeper (which will probably never be added), the emulator is pretty accurate. I've tested it with about 15 ROMs, including an 8 ROM test suite, and they all worked fine.

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
