dnl --------------------------------
dnl ibrdtn package configuration
dnl --------------------------------

define([PKG_VERSION], m4_normalize(m4_include([version.inc])))

AC_INIT(ibrdtnd, [PKG_VERSION], morgenro@ibr.cs.tu-bs.de)
AC_CONFIG_SRCDIR(src/Main.cpp)
AM_INIT_AUTOMAKE
LT_INIT
AM_PATH_CPPUNIT(1.9.6)

AC_CONFIG_HEADER([src/config.h])
AC_CONFIG_MACRO_DIR([m4])
AM_MAINTAINER_MODE

# Test for new silent rules and enable only if they are available
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

dnl -----------------------------------------------
dnl define SVN revision
dnl -----------------------------------------------
define([buildnumber], esyscmd([sh -c "git rev-parse --short HEAD|tr -d '\n'"]))dnl
AC_DEFINE(BUILD_NUMBER, "buildnumber", [build number based on the version control system])

dnl -----------------------------------------------
dnl Checks for programs.
dnl -----------------------------------------------

AC_PROG_CXX
AC_PROG_CC
AC_PROG_CPP

AC_PROG_AWK
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_LIBTOOL_DLOPEN
AC_PROG_LIBTOOL

AC_LANG_CPLUSPLUS

# ANDROID configuration
# We generate a mostly static configuration for Android as PKG_CHECK_MODULES obviously does not work with the Android NDK
AC_ARG_ENABLE([android],
	AS_HELP_STRING([--enable-android], [Enable compile switches for Android]))

AS_IF([test "x$enable_android" = "xyes"], [
	AC_MSG_WARN([Compiling for Android! Overriding other parameters!])

	# Check for presence of androgenizer
	AC_CHECK_PROG(ANDROGENIZER, androgenizer, androgenizer)
	AS_IF([test -z "$ANDROGENIZER"], [
		AC_MSG_ERROR("Unable to find androgenizer in your PATH.")
	])

	# Basic
	AC_DEFINE(ANDROID, [1], [Compiling for Android!])

	ibrdtn_LIBS="-librdtn"
	AC_SUBST(ibrdtn_LIBS)
	
	# Lowpan
	#AC_SUBST(NL_CFLAGS)
	#AC_SUBST(NL_LIBS)
	#AC_DEFINE(HAVE_LOWPAN_SUPPORT, [1], ["enable lowpan support"])
	use_lowpan="no"

	# Compression
	AC_DEFINE(WITH_COMPRESSION, [1], ["bundle compression support enabled"])
	with_compression="yes"

	# DTNSec
	AC_DEFINE(WITH_BUNDLE_SECURITY, [1], ["bundle security support enabled"])
	CPPFLAGS="$CPPFLAGS -DWITH_BUNDLE_SECURITY"
	with_dtnsec="yes"

	# TLS
	AC_DEFINE(WITH_TLS, [1], ["tls support enabled"])
	CPPFLAGS="$CPPFLAGS -DWITH_TLS"
	with_tls="yes"

	# REGEX
	AC_DEFINE(HAVE_REGEX_H, [1] ["Define to 1 if you have the <regex.h> header file."])
	have_regex="yes"

	# disabled:
	#AC_DEFINE(HAVE_LIBDAEMON, [1], ["daemon library is available"])
	#AC_DEFINE(HAVE_LIBCURL, [1], ["curl library is available"])
	#AC_DEFINE(HAVE_SQLITE, [1], ["sqlite library is available"])


	# triggers other conditionals at the end
	PDFLATEX="no"
	use_curl="no"
	use_sqlite="no"
	use_daemon="no"
	with_dht="no"
],[
#ANDROID else begin

	# Checks for libraries.
	AC_CHECK_LIB([rt], [clock_gettime])
	AC_CHECK_LIB([m], [pow])

	# check for ibrdtn library
	LOCAL_IBRDTN_VERSION=0.8
	LOCAL_CHECK_IBRDTN
	AC_SUBST(ibrdtn_CFLAGS)
	AC_SUBST(ibrdtn_LIBS)

	# Checks for header files.
	AC_CHECK_HEADERS([syslog.h pwd.h sys/inotify.h])

	# Checks for typedefs, structures, and compiler characteristics.
	AC_HEADER_STDBOOL
	AC_TYPE_SIZE_T

	# Checks for library functions.
	AC_CHECK_FUNCS([gethostname socket])

	# Check for presence of pdfLaTeX
	AC_CHECK_PROG(PDFLATEX, pdflatex, pdflatex)
	if test -z "$PDFLATEX"; then
	AC_MSG_WARN([Unable to create PDF version of the documentation.])
	fi

	PKG_CHECK_MODULES(DAEMON, libdaemon >= 0.12, [
		use_daemon="yes"
		AC_SUBST(DAEMON_CFLAGS)
		AC_SUBST(DAEMON_LIBS)
		AC_DEFINE(HAVE_LIBDAEMON, [1], ["daemon library is available"])
		AC_CHECK_LIB(daemon, daemon_reset_sigs, [
			AC_DEFINE(HAVE_DAEMON_RESET_SIGS, [1], ["daemon library has daemon_reset_sigs() and daemon_unblock_sigs() functions"])
		])
	], [
		use_daemon="no"
		AC_MSG_WARN([daemon library not found, daemonize features disabled])
	])

	AC_ARG_WITH(curl,
		AS_HELP_STRING([--with-curl], [Build with libcurl
			@<:@default=no@:>@]),
		[
		if test "x$with_curl" = "xyes"; then
			PKG_CHECK_MODULES(CURL, libcurl >= 7.16.4, [
				use_curl="yes"
				AC_SUBST(CURL_CFLAGS)
				AC_SUBST(CURL_LIBS)
				AC_DEFINE(HAVE_LIBCURL, [1], ["curl library is available"])
			], [
				use_curl="no"
				AC_MSG_WARN([curl extension requested, but no curl library is found.])
			])
		else
			use_curl="no"
		fi
		],[
		use_curl="no"
	])

	AC_ARG_WITH(sqlite,
		AS_HELP_STRING([--with-sqlite], [Build with sqlite support
			@<:@default=no@:>@]),
		[
		if test "x$with_sqlite" = "xyes"; then
			PKG_CHECK_MODULES(SQLITE, sqlite3 >= 3.6, [
				use_sqlite="yes"
				AC_SUBST(SQLITE_CFLAGS)
				AC_SUBST(SQLITE_LIBS)
				AC_DEFINE(HAVE_SQLITE, [1], ["sqlite library is available"])
			], [
				use_sqlite="no"
				AC_MSG_WARN([sqlite extension requested, but no sqlite library is found.])
			])
		else
			use_sqlite="no"
		fi
		],[
		use_sqlite="no"
	])

	# Checks for lowpan support (libnl)
	AC_ARG_WITH(lowpan,
		AS_HELP_STRING([--with-lowpan], [Build with lowpan support (support in ibrcommon needed)
			@<:@default=no@:>@]),
		[
		if test "x$with_lowpan" = "xyes"; then
			PKG_CHECK_MODULES([NL], [libnl-3.0], [
				use_lowpan="yes"
				AC_SUBST(NL_CFLAGS)
				AC_SUBST(NL_LIBS)
				AC_DEFINE(HAVE_LOWPAN_SUPPORT, [1], ["enable lowpan support"])
				AC_CHECK_HEADERS([arpa/inet.h])
				AC_CHECK_HEADERS([fcntl.h])
				AC_CHECK_HEADERS([limits.h])
				AC_CHECK_HEADERS([netinet/in.h])
				AC_CHECK_HEADERS([sys/socket.h])
			], [
				PKG_CHECK_MODULES([NL], [libnl-1], [
					use_lowpan="yes"
					AC_SUBST(NL_CFLAGS)
					AC_SUBST(NL_LIBS)
					AC_DEFINE(HAVE_LOWPAN_SUPPORT, [1], ["enable lowpan support"])
					AC_CHECK_HEADERS([arpa/inet.h])
					AC_CHECK_HEADERS([fcntl.h])
					AC_CHECK_HEADERS([limits.h])
					AC_CHECK_HEADERS([netinet/in.h])
					AC_CHECK_HEADERS([sys/socket.h])
				], [
					use_lowpan="no"
					AC_MSG_WARN([lowpan extension requested, but no libnl library is found. lowpan support is disabled.])
				])
			])
		else
			use_lowpan="no"
		fi
		],[
		use_lowpan="no"
	])

	dnl -----------------------------------------------
	dnl optional parameter: compression support
	dnl -----------------------------------------------

	AC_ARG_WITH(compression,
		AS_HELP_STRING([--with-compression], [build with compression support
			@<:@default=no@:>@]),
		[
		if test "x$with_compression" = "xyes"; then
			AC_DEFINE(WITH_COMPRESSION, [1], ["bundle compression support enabled"])
		else
			with_compression="no"
		fi
		],[
		with_compression="no"
	])


	dnl -----------------------------------------------
	dnl optional parameter: DTN security support
	dnl -----------------------------------------------

	AC_ARG_WITH(dtnsec,
		AS_HELP_STRING([--with-dtnsec], [build with DTNSEC support
			@<:@default=no@:>@]),
		[
		if test "x$with_dtnsec" = "xyes"; then
			AC_DEFINE(WITH_BUNDLE_SECURITY, [1], ["bundle security support enabled"])
			CPPFLAGS="$CPPFLAGS -DWITH_BUNDLE_SECURITY"
		else
			with_dtnsec="no"
		fi
		],[
		with_dtnsec="no"
	])


	dnl -----------------------------------------------
	dnl optional parameter: TLS support
	dnl -----------------------------------------------

	AC_ARG_WITH(tls,
		AS_HELP_STRING([--with-tls], [build with TLS support
			@<:@default=no@:>@]),
		[
		if test "x$with_tls" = "xyes"; then
			AC_DEFINE(WITH_TLS, [1], ["tls support enabled"])
			CPPFLAGS="$CPPFLAGS -DWITH_TLS"
		else
			with_tls="no"
		fi
		],[
		with_tls="no"
	])


	dnl -----------------------------------------------
	dnl optional parameter: DHT support
	dnl -----------------------------------------------

	PKG_CHECK_MODULES([DTNDHT], dtndht >= 0.2.2 , [
				with_dht="yes"
				AC_SUBST(DTNDHT_CFLAGS)
				AC_SUBST(DTNDHT_LIBS)
				AC_DEFINE(WITH_DHT_NAMESERVICE, [1], ["dht nameservice support enabled"])
				AC_CHECK_HEADERS([arpa/inet.h])
				AC_CHECK_HEADERS([fcntl.h])
				AC_CHECK_HEADERS([netinet/in.h])
				AC_CHECK_HEADERS([sys/socket.h])
			], [
				with_dht="no"
				AC_MSG_WARN([dtndht library not found. dht support is disabled.])
			])


	dnl -----------------------------------------------
	dnl optional parameter: Wifi-P2P support
	dnl -----------------------------------------------

	PKG_CHECK_MODULES([WIFIP2P], wifip2p >= 0.1 , [
				with_wifi_p2p="yes"
				AC_SUBST(WIFIP2P_CFLAGS)
				AC_SUBST(WIFIP2P_LIBS)
				AC_DEFINE(WITH_WIFIP2P, [1], ["wifi-p2p support enabled"])
			], [
				with_wifi_p2p="no"
				AC_MSG_WARN([wifi-p2p library not found. wifi-p2p support is disabled.])
			])

    dnl -----------------------------------------------
    dnl optional parameter: VMime support
    dnl -----------------------------------------------

    PKG_CHECK_MODULES([VMIME], vmime >= 0.9.1 , [
                with_vmime="yes"
                AC_SUBST(VMIME_CFLAGS)
                AC_SUBST(VMIME_LIBS)
                AC_DEFINE(HAVE_VMIME, [1], ["Email Convergence Layer is available"])
                AC_CHECK_HEADERS([vmime/utility/smartPtrInt.hpp])
            ], [
                with_vmime="no"
                AC_MSG_WARN([VMime library not found. Email Convergence Layer is disabled.])
            ])

	dnl -----------------------------------------------
	dnl check for regex capabilities
	dnl -----------------------------------------------
	AC_CHECK_HEADERS([regex.h], [
		have_regex="yes"
	], [
		have_regex="no"
	])

#ANDROID else end
])

AM_CONDITIONAL(HAVE_PDFLATEX, test x$PDFLATEX = xpdflatex)
AM_CONDITIONAL(COMPRESSION, test x$with_compression = xyes)
AM_CONDITIONAL(CURL, test x$use_curl = xyes)
AM_CONDITIONAL(SQLITE, test x$use_sqlite = xyes)
AM_CONDITIONAL(LOWPAN, test x$use_lowpan = xyes)
AM_CONDITIONAL(DAEMON, test x$use_daemon = xyes)
AM_CONDITIONAL(DTNSEC, test x$with_dtnsec = xyes)
AM_CONDITIONAL(TLS, test x$with_tls = xyes)
AM_CONDITIONAL(DTNDHT, test x$with_dht = xyes)
AM_CONDITIONAL(REGEX, test x$have_regex = xyes)
AM_CONDITIONAL(ANDROID, test x$enable_android = xyes)
AM_CONDITIONAL(WIFIP2P, test x$with_wifi_p2p = xyes)
AM_CONDITIONAL(VMIME, test x$with_vmime = xyes)

dnl -----------------------------------------------
dnl Generates Makefiles, configuration files and scripts
dnl -----------------------------------------------

AC_CONFIG_FILES([Makefile \
	src/Makefile \
	src/core/Makefile \
	src/net/Makefile \
	src/storage/Makefile \
	src/routing/Makefile \
	src/routing/epidemic/Makefile \
	src/routing/flooding/Makefile \
	src/routing/prophet/Makefile \
	src/security/Makefile \
	src/api/Makefile \
	etc/Makefile \
	doc/Makefile \
	man/Makefile \
	scripts/Makefile \
	munin/Makefile \
	tests/Makefile \
	tests/unittests/Makefile])
	
AC_OUTPUT
