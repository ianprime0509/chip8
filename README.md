# Chip-8

This is a [Chip-8](https://en.wikipedia.org/wiki/CHIP-8) emulator, written in
C.  Currently, all the Chip-8 games I have tested are functional, and some
Super-Chip games are as well (in particular, BLINKY).  Many Super-Chip games
don't quite work yet, though.

The main documentation for the project is as a set of manual (man) pages,
located in the `man` directory.  They are written using the `mdoc` macros,
which are widely used in the BSD world but not so much with GNU/Linux.  If for
some reason you have trouble viewing them on your system, you may want to
install the [mandoc](http://mandoc.bsd.lv/) program, which can convert them to
various formats, including HTML.

## Building

You can build the project using the following commands (the leading `$` is your
prompt):

```shell
$ mkdir build && cd build
$ meson
$ ninja
```

The executable binaries will be located in `build/src`.

## License

This is free software, distributed under the [MIT
license](https://opensource.org/licenses/MIT).
