# To build using autotools, see the Development section of the file INSTALL.

AC_PREREQ([2.59])
AC_INIT([ndn-av1], [0.1], [apps@lists.cs.ucla.edu], [ndn-av1], [https://github.com/remap/aom])
AM_INIT_AUTOMAKE([1.10 -Wall no-define foreign])

AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([configure.ac])
# Create config.h without prefixes.
AC_CONFIG_HEADERS(src/config.h)
# Add prefixes and output to a unique file name.
AX_PREFIX_CONFIG_H(src/ndn-av1-config.h)
AM_MAINTAINER_MODE
AM_PROG_AR
AC_PROG_LIBTOOL
LT_PREREQ([2.2])
LT_INIT()
AC_PROG_CXX
AM_PROG_CC_C_O
AC_PROG_OBJCXX
AC_LANG([C++])

# Special case.
case "$host" in
  *-darwin* | *-macos10*)
     if test -d /opt/local ; then
       CFLAGS="$CFLAGS -I/opt/local/include"
       CXXFLAGS="$CXXFLAGS -I/opt/local/include"
       CPPFLAGS="$CPPFLAGS -I/opt/local/include"
       LDFLAGS="$LDFLAGS -L/opt/local/lib"
     elif test -d /sw ; then
       CFLAGS="$CFLAGS -I/sw/include"
       CXXFLAGS="$CXXFLAGS -I/sw/include"
       CPPFLAGS="$CPPFLAGS -I/sw/include"
       LDFLAGS="$LDFLAGS -L/sw/lib"
     fi
  ;;
esac

AC_ARG_VAR(ADD_CFLAGS, [Append this to CFLAGS])
CFLAGS="$CFLAGS $ADD_CFLAGS"
AC_ARG_VAR(ADD_CXXFLAGS, [Append this to CXXFLAGS])
CXXFLAGS="$CXXFLAGS $ADD_CXXFLAGS"
AC_ARG_VAR(ADD_CPPFLAGS, [Append this to CPPFLAGS])
CPPFLAGS="$CPPFLAGS $ADD_CPPFLAGS"
AC_ARG_VAR(ADD_LDFLAGS, [Append this to LDFLAGS])
LDFLAGS="$LDFLAGS $ADD_LDFLAGS"

AC_TYPE_SIZE_T
AC_TYPE_INT8_T
AC_TYPE_INT16_T
AC_TYPE_INT32_T
AC_TYPE_INT64_T
AC_TYPE_UINT8_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_CHECK_FUNCS([memcmp] [memcpy] [memset])
AC_CHECK_FUNCS([strchr], :, AC_MSG_ERROR([*** strchr not found. Check 'config.log' for more details.]))
AC_CHECK_FUNCS([sscanf], :, AC_MSG_ERROR([*** sscanf not found. Check 'config.log' for more details.]))
AC_CHECK_HEADERS([time.h], :, AC_MSG_ERROR([*** time.h not found. Check 'config.log' for more details.]))
AC_CHECK_HEADERS([sys/time.h], :, AC_MSG_ERROR([*** sys/time.h not found. Check 'config.log' for more details.]))
AC_CHECK_FUNCS([gettimeofday], :, AC_MSG_ERROR([*** gettimeofday not found. processEvents requires it. Check 'config.log' for more details.]))
AC_CHECK_FUNCS([round])
AX_CXX_COMPILE_STDCXX_11(, optional)
AC_C_INLINE

# Use the minimum boost version from NFD.
AX_BOOST_BASE([1.48])

AX_BOOST_ASIO

# Set AOM_BUILD_DIR from --with-aom-builddir, or use the default.
AC_ARG_WITH([aom-builddir],
    [AS_HELP_STRING([--with-aom-builddir=/path/of/build/dir],
        [Location when you did make for aom. (Not aom/build from the distribution.)]
        [Defaults to ../aom_build .])],
    [aom_build_dir="$withval"], [aom_build_dir="../aom_build"]
)
# The Makefile will add AOM_BUILD_DIR to the header search path.
AC_SUBST(AOM_BUILD_DIR, ${aom_build_dir})

# Require our modified libaom which has gPacketizerMode. Use the library in the
# build directory so that we don't require the user to install the hacked version.
LDFLAGS="-L${aom_build_dir} ${LDFLAGS}"
AC_CHECK_LIB([aom], [gPacketizerMode], [],
             [AC_MSG_FAILURE([can't find the aom library with gPacketizerMode])])

# Require libcrypto.
AC_CHECK_LIB([crypto], [EVP_EncryptInit], [],
             [AC_MSG_FAILURE([can't find openssl crypto lib])])

# Check for RocksDB
AC_CHECK_HEADERS([rocksdb/db.h],,[AC_MSG_FAILURE([can't find RocksDB])])
AC_CHECK_LIB([rocksdb], [rocksdb_restore_options_create])

# Require libndn-cpp.
AC_CHECK_LIB([ndn-cpp], [ndn_getErrorString], [],
             [AC_MSG_FAILURE([can't find the ndn-cpp library])])

# Require libndn-cpp-tools.
AC_CHECK_LIB([ndn-cpp-tools], [ndntools_getGeneralizedContentMetaComponent], [],
             [AC_MSG_FAILURE([can't find the ndn-cpp-tools library])])

# Require libcnl-cpp.
AC_CHECK_LIB([cnl-cpp], [cnl_cpp_getSegmentStreamHandlerManifestComponent], [],
             [AC_MSG_FAILURE([can't find the cnl-cpp library])])

DX_HTML_FEATURE(ON)
DX_CHM_FEATURE(OFF)
DX_CHI_FEATURE(OFF)
DX_MAN_FEATURE(OFF)
DX_RTF_FEATURE(OFF)
DX_XML_FEATURE(OFF)
DX_PDF_FEATURE(OFF)
DX_PS_FEATURE(OFF)
DX_INIT_DOXYGEN([$PACKAGE_NAME],[Doxyfile])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
