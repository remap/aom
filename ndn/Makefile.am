include aminclude.am

## Place generated object files (.o) into the same directory as their source
## files, in order to avoid collisions when non-recursive make is used.
AUTOMAKE_OPTIONS = subdir-objects

## Additional flags to pass to aclocal when it is invoked automatically at
## make time. The ${ACLOCAL_FLAGS} variable is picked up from the environment
## to provide a way for the user to supply additional arguments.
ACLOCAL_AMFLAGS = ${ACLOCAL_FLAGS} -I m4

AOM_INCLUDES = -I.. -I../third_party/libwebm \
  -I../third_party/googletest/src/googletest \
  -I../third_party/googletest/src/googletest/include
AM_CPPFLAGS=${BOOST_CPPFLAGS} ${AOM_INCLUDES} -I${AOM_BUILD_DIR}
AM_LDFLAGS=${BOOST_LDFLAGS} ${BOOST_SYSTEM_LIB} ${BOOST_REGEX_LIB} ${BOOST_ASIO_LIB}

lib_LTLIBRARIES = libndn-av1.la

noinst_PROGRAMS = bin/fetch-tiles bin/store-tiles

# Files from aom that are not part of libaom.a .
libndn_av1_la_SOURCES = \
  ../common/args.c ../common/args.h \
  ../common/av1_config.c ../common/av1_config.h \
  ../common/ivfdec.c ../common/ivfdec.h \
  ../common/ivfenc.c ../common/ivfenc.h \
  ../common/md5_utils.c ../common/md5_utils.h \
  ../common/obudec.c ../common/obudec.h \
  ../common/rawenc.c ../common/rawenc.h \
  ../common/tools_common.c ../common/tools_common.h \
  ../common/video_common.h \
  ../common/video_reader.c ../common/video_reader.h \
  ../common/video_writer.c ../common/video_writer.h \
  ../common/warnings.c ../common/warnings.h \
  ../common/webmdec.cc ../common/webmdec.h \
  ../common/webmenc.cc ../common/webmenc.h \
  ../common/y4menc.c ../common/y4menc.h \
  ../common/y4minput.c ../common/y4minput.h \
  ../third_party/libwebm/common/file_util.cc ../third_party/libwebm/common/file_util.h \
  ../third_party/libwebm/common/hdr_util.cc ../third_party/libwebm/common/hdr_util.h \
  ../third_party/libwebm/common/webmids.h \
  ../third_party/libwebm/mkvmuxer/mkvmuxer.cc ../third_party/libwebm/mkvmuxer/mkvmuxer.h \
  ../third_party/libwebm/mkvmuxer/mkvmuxertypes.h \
  ../third_party/libwebm/mkvmuxer/mkvmuxerutil.cc ../third_party/libwebm/mkvmuxer/mkvmuxerutil.h \
  ../third_party/libwebm/mkvmuxer/mkvwriter.cc ../third_party/libwebm/mkvmuxer/mkvwriter.h \
  ../third_party/libwebm/mkvparser/mkvparser.cc ../third_party/libwebm/mkvparser/mkvparser.h \
  ../third_party/libwebm/mkvparser/mkvreader.cc ../third_party/libwebm/mkvparser/mkvreader.h

bin_fetch_tiles_SOURCES = src/fetch-tiles.cpp src/packetizer-from-ndn.cpp
bin_fetch_tiles_LDADD = libndn-av1.la

bin_store_tiles_SOURCES = src/store-tiles.cpp contrib/fast-repo/storage-engine.cpp
bin_store_tiles_LDADD = libndn-av1.la

dist_noinst_SCRIPTS = autogen.sh
