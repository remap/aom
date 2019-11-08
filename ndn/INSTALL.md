NDN extensions for the aom AV1 decoder
======================================

Prerequisites
=============
* Required: NDN-CPP (https://github.com/named-data/ndn-cpp)
* Required: CNL-CPP (https://github.com/named-data/cnl-cpp)
* Required: libcrypto
* Required: Protobuf (for generalized objects)
* Required: cmake (to build libaom)
* Optional: NFD (for fetch-tiles to connect to a local forwarder)
* Optional: fast-repo (as the repo for fetch-tiles. https://github.com/remap/fast-repo)
* Optional: libsqlite3 (for key storage in NDN-CPP)
* Optional: OSX Security framework (for key storage in NDN-CPP)
* Optional: log4cxx (for debugging and log output in NDN-CPP and CNL-CPP)
* Optional: Boost (min version 1.48) with asio (for ThreadsafeFace and async I/O in NDN-CPP)
* Optional: SvtAv1EncApp (for creating AV1 files. https://github.com/OpenVisualCloud/SVT-AV1)
* Optional: librocksdb (for store-tiles to store Data packets in the repo)
* Optional: ffmpeg (for ffplay to play the raw video file from fetch-tiles)

The steps to install most of the prerequisites are the same as to build and install NDN-CPP.
Instructions for the remaining prerequisites are below.
Please see https://github.com/named-data/ndn-cpp/blob/master/INSTALL.md .

[Ubuntu only] After installing NDN-CPP and CNL-CPP, be sure to update the path to the
shared libraries using `sudo /sbin/ldconfig` .

Optional: SvtAv1EncApp, must be built on Linux or Windows. It doesn't seem to support macOS.
Follow the instructions at https://github.com/OpenVisualCloud/SVT-AV1

## macOS 10.12, macOS 10.13 and macOS 10.14
In a terminal, enter:

    brew install cmake nasm

Optional: To install librocksdb, in a terminal, enter:

    brew install rocksdb

Optional: To install ffmpeg, in a terminal, enter:

    brew install ffmpeg

Build
=====
This project is a fork of the aom project which has been modified to store and read individual tiles.
You must build libaom in this fork (instead of using another distribution. In the following,
`<aom-root>` is the root of this fork of the aom distribution. (Make sure that it is on the ndn branch.)
  
To build in a terminal, change directory to `<aom-root>` and enter:

    mkdir aom_build
    cd aom_build

To run cmake on macOS, enter the following. (This specifies the nasm installed by brew,
which overrides macOS's old `/usr/bin/nasm` which can't be removed.)

    cmake .. -DAS_EXECUTABLE:FILEPATH=/usr/local/bin/nasm -DENABLE_TESTS=0

To run cmake on other systems, enter:

    cmake .. -DENABLE_TESTS=0

To finish building libaom, enter:

    make

To build the store and fetch tools, change directory to `<aom-root>/ndn` .

To configure on macOS, enter:

    ./configure ADD_CFLAGS=-I/usr/local/opt/openssl/include ADD_CXXFLAGS=-I/usr/local/opt/openssl/include ADD_LDFLAGS=-L/usr/local/opt/openssl/lib

To configure on other systems, enter:

    ./configure

To build the tools, enter:

    make

Files
=====
This makes the following example programs:

* bin/store-tiles: Parse an AV1 video file, create NDN Data packets and put them in the repo. For help, run with no arguments.
* bin/fetch-tiles: Fetch and decode NDN Data packets (selected tiles) and output raw video. Requires a local running NFD. For help, run with no arguments.

This does not make a library for fetching NDN tiles. Instead, your application should
compile and link the modified files from aom, and use the PacketizerFromNdn class, 
like the fetch-tiles application.

Usage
=====
You need an AV1 video file that uses tiling, and is encoded so that the tiles are independent
(so that the decoder functions when only some tiles are fetched). You can use SvtAv1EncApp.
(This only works on Windows and Linux, so if you are on macOS, then you must temporarily transfer
your files to another computer.) In the following example, `myvideo.y4m` is the input raw video file
which is 3840x2160 resolution with 50 frames per second. The actual number of tile columns and rows
is 2^N, so the following has 8 columns and 4 rows. The `-umv 0` option turns off "unrestricted
motion vectors" so that the tiles are independently encoded. Finally `myvideo_8x4.ivf` is the output
AV1 video file (which is required to have the `.ivf` extension). Don't forget the `-b` before the output
file name. Note that AV1 encoding can take hours.

    SvtAv1EncApp -i myvideo.y4m -w 3840 -h 2160 -fps 50 -tile-columns 3 -tile-rows 2 -umv 0 -b myvideo_8x4.ivf

To process the AV1 video file and store the tile packets in the repo, use `bin/store-tiles` which you built above.
(Make sure that `fast-repo` is not running, since `store-files` stores directly into the repo.) For example,
the following processes the AV1 video file `myvideo_8x4.ivf`, creates NDN Data packets with the prefix
`/ndn/myvideo` and stores them in the repo at `$HOME/fast-repo`. (This signs the Data packets using the
default identity, the same as the NFD tools.)

    bin/store-tiles myvideo_8x4.ivf /ndn/myvideo $HOME/fast-repo

Now the packets are in the repo. To fetch them, start NFD and fast-repo. For example, in a different
terminal, enter:

    nfd-start
    fast-repo /ndn/fast-repo --db-path=$HOME/fast-repo

The fetch command is `fetch-tiles <prefix> <outfile> [<row>,<col>] [<row>,<col>] ...` . 
For example, the following fetches the video, decoding only one tile at row 2 and column 4, 
and saves to the raw video file `myvideo-1-tile.y4m` :

    bin/fetch-tiles /ndn/myvideo myvideo-1-tile.y4m 2,4

When finished it prints the following command which you can use to view the raw video file.

    ffplay -f rawvideo -pix_fmt yuv420p -s 3840x2160 -framerate 50 myvideo-1-tile.y4m
