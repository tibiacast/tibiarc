# tibiarc

`tibiarc` is a library that can be used to convert Tibia packet captures to
video files without requiring a Tibia client, making it suitable for unattended
conversions. Its core was once part of Tibiacast but it has been broken out
and released as open source to aid the preservation of old Tibia recordings.

The state of the code is largely the same as when Tibiacast closed in 2016, but
I've tried to clean things up a bit to make it useful as a library. I've also
added a basic translation layer that lets it support arbitrary Tibia versions
as long as the appropriate `.dat`/`.spr`/`.pic` files are present. If you
cannot find a `.pic` file, you can safely use one from a later Tibia version.

There's currently support for all versions between 7.11 and 7.92, but others
may be hit or miss as I haven't had enough files to test with.

I hope someone finds this useful. :-)

/John

## Thanks

Many thanks to [gurka](https://github.com/gurka) for reverse-engineering the
TibiCAM `.rec` format and providing a [tool for converting] it to a saner
format, to [gesior](https://github.com/gesior) for maintaining a
[repository of `.dat` and `.spr` files] for old versions, and the TibiaBR staff
for hosting a public archive of `.rec` files to test with: this release would
not have happened without you.

[tool for converting]: https://github.com/gurka/OldSchoolTibia
[repository of `.dat` and `.spr` files]: https://downloads.ots.me/?dir=data/tibia-clients/dat_and_spr_collections

## Building

This project was originally developed on and for Windows, but as I've abandoned
that platform altogether I've been unable to test there, so this only works out
of the box on Unix-like systems at the moment. It should work fine under WSL
however, and it's possible to cross-compile it with MXE or similar.

First, make sure that you have all dependencies:

* `binutils`
* `clang` or `gcc`
* `cmake`
* `libavcodec`
* `libavformat`
* `libavutil`
* `libsdl2`
* `libswscale`
* `make`
* `pkg-config`
* `zlib`

The following should install everything on Debian and derived distros like
Ubuntu:

```
$ sudo apt install binutils clang cmake libavcodec-dev libavformat-dev libavutil-dev libsdl2-dev libswscale-dev make pkg-config zlib1g-dev
```

Then jump into the project folder and run the following commands, if all goes
well, it'll create the `libtibiarc.a` library and demo `converter` program in
that folder.

```
$ mkdir build
$ cd build
$ cmake ..
$ make
```

### Cross-compilation to Windows

First, build the MXE cross-compilation environment if you haven't already, and
set up the required packages:

```
$ git clone https://github.com/mxe/mxe.git
$ cd mxe
$ make -j8 x264 x265 ffmpeg sdl2 zlib MXE_TARGETS='x86_64-w64-mingw32.static'
```

Then build `tibiarc` with the MXE toolchain:

```
$ export PATH=$MXE_DIRECTORY/usr/bin:$PATH
$ mkdir build
$ cd build
$ x86_64-w64-mingw32.static-cmake -DTIBIARC_CROSSCOMPILING=On ..
$ make
```

Assuming this worked, you should now have a `tibiarc.exe` in the build folder
with all dependencies statically linked. The command line interface is a bit
broken however, due to the `argp` library being missing in `MXE`. I'll fix that
when I find the time.

## Usage

    ./converter [options] data_folder input_file output_file

Where `data_folder` points at folder with appropriate data files (`Tibia.dat`,
`Tibia.spr`, `Tibia.pic`) for the recording (`input_file`) and `output_file` is
the desired output file.

The converter will try to guess the input format, but if it fails you can use
the `--input-format` flag to force the right format. `tibiacast`, `tmv2`
(Tibia-Movie), `trp` (TibiaReplay), and `yatc` files are supported at present.

By default the output format is guessed by the file extension, but
`--output-format` can be used to select a specific one, e.g. `matroska`.
`--output-encoding` can also be used to pick a certain codec if you don't want
the format's default.

For more control, `--output-flags` can be used to provide largely the same
options as you would to `ffmpeg` on the command line (just skip the dashes and
mind the `key1=value1:key2=value2` convention), except video filters aren't
supported yet. This can be used to set up things like the screenshot mode
detailed below, network streaming, custom encoding profiles, et cetera.

For a full list of options, run `./converter --help`.

### Screenshots

The converter can also be used to generate screenshots and thumbnails at a
given point in time by using the `image2` output format in `update` mode:

    ./converter --input-format tmv2 \
                --output-format image2 \
                --output-encoding bmp \
                --output-flags "update=1" \
                --resolution 1440x1056 \
                --start-time 1234 \
                --end-time 1234 \
                --frame-rate 1 \
                data/folder \
                tests/8.40/sample.tmv2 \
                converted/sample_1234.bmp

Combining this with the `--frame-skip N` option which limits encoding to one in
every `N` frames, we can generate a series of "preview" images for a recording,
using `image2`s sequence feature.

For example, the following generates a series of one image every three seconds
of the given recording:

    ./converter --input-format tmv2 \
                --output-format image2 \
                --output-encoding bmp \
                --resolution 480x352 \
                --frame-rate 1 \
                --frame-skip 3 \
                data/folder \
                tests/8.40/sample.tmv2 \
                converted/sample_previews_%03d.bmp

Furthermore, by tweaking the render options we can remove effects (like text)
that cause clutter and look awful when scaled down to the size of a thumbnail:

    ./converter --input-format tmv2 \
                --output-format image2 \
                --output-encoding bmp \
                --output-flags "update=1" \
                --resolution 128x96 \
                --start-time 1234 \
                --end-time 1234 \
                --frame-rate 1 \
                --skip-rendering-inventory \
                --skip-rendering-icon-bar \
                --skip-rendering-status-bars \
                --skip-rendering-messages \
                --skip-rendering-player-names \
                --skip-rendering-creature-health-bars \
                --skip-rendering-creature-names \
                --skip-rendering-creature-icons \
                data/folder \
                tests/8.40/sample.tmv2 \
                converted/sample_thumbnail.bmp

I've used `bmp` in these examples but largely any format supported by your
`ffmpeg` install will work (`.jpg` and `.webp` are often included).

### Player mode

The `sdl2` output can be used to output the video to the screen instead of a
file. It's not very useful as a video player but it does give quicker
turnaround time for troubleshooting render bugs.

    ./converter --output-backend sdl2 \
                --resolution 1440x1056 \
                data/folder \
                tests/8.40/sample.tmv2

### Unsupported formats

Classic TibiCAM `.rec` files are not yet supported due to the format being
designed to be annoying on purpose. I haven't had the time to implement it,
but they can still be viewed by converting them to a more convenient format.

[TibiaReplay] can be used to convert recordings up until Tibia version 7.92.

[TibiaTimeMachine] seems capable of converting files of any version, and also
supports other formats like `.cam` and `.byn`, but there's one small problem:
we don't yet support the `.ttm` files it produces. Adding support for that
would be trivial as it's open source, but I don't have any files to test with,
feel free to send me some. :)

[TibiaReplay]: https://github.com/gurka/OldSchoolTibia/
[TibiaTimeMachine]: https://github.com/tulio150/tibia-ttm/

## Testing

The tests can be run by executing `make` in the `tests/` directory, but note
that they require appropriate data files in each versioned folder (that is,
make sure `tests/8.50/Tibia.spr` and so on are present), which we aren't free
to redistribute as part of the repository. If you don't already have those, you
can find them in [gesior's `.dat/.spr` collection].

Further files to test with are much appreciated, especially if you recorded
them yourself and grant us a license to include them in this repository.

[gesior's `.dat/.spr` collection]: https://downloads.ots.me/?dir=data/tibia-clients/dat_and_spr_collections

## License

This software is distributed under the terms of the GNU Affero General Public
License as published by the Free Software Foundation, either version 3 of the
License, or (at your option) any later version. See the `COPYING` file for
details.

When applicable, the copyright notices in this software expresses ranges of
"copyrightable" years as `StartYear-EndYear`, e.g. `2014-2016` means the years
`2014`, `2015`, and `2016`.
