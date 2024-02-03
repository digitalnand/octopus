# Usage
build the project
```bash
just build release
```
provide bin/octop with a valid CHIP-8 ROM, ending with a ".ch8" extension
```bash
bin/octop roms/br8kout.ch8
```
# Limitations
- currently, it only supports the instructions specified in the technical reference used, and does not support quirks
# References
- [Chip-8 Technical Reference](http://devernay.free.fr/hacks/chip8/C8TECH10.HTM)
