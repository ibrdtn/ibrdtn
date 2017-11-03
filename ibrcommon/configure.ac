dnl --------------------------------
dnl Package name and version number
dnl --------------------------------

# include version information
m4_include([version.m4])

dnl --------------------------------
dnl Initialization macros.
dnl --------------------------------

AC_INIT([ibrcommon], PKG_FULL_VERSION, morgenro@ibr.cs.tu-bs.de)
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR(ibrcommon/Logger.cpp)
AC_CONFIG_HEADER([ibrcommon/config.h ibrcommon/ibrcommon.h])
LT_INIT
AM_PATH_CPPUNIT(1.9.6)
AM_INIT_AUTOMAKE
AM_MAINTAINER_MODE

# Test for new silent rules and enable only if they are available
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

dnl --------------------------------
dnl library version number
dnl --------------------------------

#API version
GENERIC_API_VERSION=PKG_MAJOR_VERSION.PKG_MINOR_VERSION

#shared library versioning
GENERIC_LIBRARY_VERSION=PKG_LIB_VERSION
#                       | | |
#                +------+ | +---+
#                |        |     |
#             current:revision:age
#                |        |     |
#                |        |     +- increment if interfaces have been added
#                |        |        set to zero if interfaces have been removed
#                                  or changed
#                |        +- increment if source code has changed
#                |           set to zero if current is incremented
#                +- increment if interfaces have been added, removed or changed

# define the library name
GENERIC_LIBRARY_NAME=$PACKAGE_NAME
GENERIC_VERSION=$PACKAGE_VERSION

AC_SUBST(GENERIC_API_VERSION)
AC_SUBST(GENERIC_LIBRARY_VERSION)
AC_SUBST(GENERIC_LIBRARY_NAME)
AC_SUBST(GENERIC_VERSION)

dnl Drops autotools defaults but keeps user-specific options.
: ${CFLAGS=""}
: ${CXXFLAGS=""}

dnl -----------------------------------------------
dnl Checks for programs.
dnl -----------------------------------------------

AC_PROG_CXX
AC_PROG_CC

AC_PROG_INSTALL
AC_LIBTOOL_DLOPEN
AC_PROG_LIBTOOL
AM_SANITY_CHECK

AC_LANG_CPLUSPLUS

# check if we build for win32
MINGW_AC_BUILD([
	AC_MSG_NOTICE([Compiling for Win32])
	
	AC_DEFINE(WIN32, [1], [Compiling for Win32!])
	AC_DEFINE(WINVER, [0x0601], [Minimum platform is Windows 7])
	AC_DEFINE(_WIN32_WINNT, [0x0601], [Minimum platform is Windows 7])

	# do win32 related library checking
	AC_CHECK_LIB([ws2_32], [main], [
		LIBS="$LIBS -lws2_32"
		WIN32_LIBS_EXTRA="-lws2_32"
		AC_SUBST(WIN32_LIBS_EXTRA)
	], AC_ERROR([lib ws2_32 not found]))

	AC_CHECK_LIB([iphlpapi], [main], [
		LIBS="$LIBS -liphlpapi"
		WIN32_LIBS_EXTRA="-liphlpapi"
		AC_SUBST(WIN32_LIBS_EXTRA)
	], AC_ERROR([lib iphlpapi not found]))
])

# ANDROID configuration
ANDROID_AC_BUILD([
	AC_MSG_NOTICE([Compiling for Android])

	# Check for presence of androgenizer
	AC_CHECK_PROG(ANDROGENIZER, androgenizer, androgenizer)
	AS_IF([test -z "$ANDROGENIZER"], [
		AC_MSG_ERROR("Unable to find androgenizer in your PATH.")
	])
	
	# Headers are defined in Application.mk, as it is impossible to link from here
	
	# Basic
	AC_DEFINE(ANDROID, [1], [Compiling for Android!])
	
	# Netlink lib
	AC_DEFINE(HAVE_LIBNL3, [1], [])
	AC_DEFINE(HAVE_LIBNL3_ROUTE, [1], [])
	AC_DEFINE(HAVE_LIBNL3_GENL, [1], [])
	NL_LIBS="-lnl-3"
	AC_SUBST(NL_LIBS)
	NL_ROUTE_LIBS="-lnl-route-3"
	AC_SUBST(NL_ROUTE_LIBS)
	NL_GENL_LIBS="-lnl-genl-3"
	AC_SUBST(NL_GENL_LIBS)
	use_netlink="yes"
	
	# OpenSSL
	AC_DEFINE(HAVE_OPENSSL, [1], ["openssl library is available"])
	AC_DEFINE(IBRCOMMON_SUPPORT_SSL, [1], ["ssl extension is available"])
	OPENSSL_LIBS="-lssl -lcryptox"
	AC_SUBST(OPENSSL_LIBS)
	use_openssl="yes"

	# Lowpan
	#AC_DEFINE(HAVE_LOWPAN_SUPPORT, [1], [])
	use_lowpan="no"

	# disabled:
	#AC_DEFINE(HAVE_LIBNL, [1], [])
	#AC_DEFINE(HAVE_PTHREAD_CONDATTR_SETCLOCK, [1], [])
	#AC_DEFINE(HAVE_PTHREAD_DELAY, [1], [])
	#AC_DEFINE(HAVE_PTHREAD_DELAY_NP, [1], [])
	#AC_DEFINE(HAVE_PTHREAD_SETCONCURRENCY, [1], [])
	#AC_DEFINE(HAVE_PTHREAD_SETSCHEDPRIO, [1], [])
	#AC_DEFINE(HAVE_PTHREAD_YIELD, [1], [])
	#AC_DEFINE(HAVE_LIBXML, [1], [])

	# triggers other conditionals at the end
	use_xml="no"
],[
#ANDROID else begin
	AC_CHECK_LIB(rt, clock_gettime, [])
	
	# Checks for header files.
	AC_CHECK_FUNCS([bzero])
	AC_CHECK_FUNCS([clock_gettime])
	AC_CHECK_FUNCS([gethostname])
	AC_CHECK_FUNCS([gettimeofday])
	AC_CHECK_FUNCS([inet_ntoa])
	AC_CHECK_FUNCS([memset])
	AC_CHECK_FUNCS([mkdir])
	AC_CHECK_FUNCS([pow])
	AC_CHECK_FUNCS([rmdir])
	AC_CHECK_FUNCS([socket])
	AC_CHECK_HEADERS([arpa/inet.h])
	AC_CHECK_HEADERS([fcntl.h])
	AC_CHECK_HEADERS([netdb.h])
	AC_CHECK_HEADERS([netinet/in.h])
	AC_CHECK_HEADERS([sys/socket.h])
	AC_CHECK_HEADERS([sys/time.h])
	AC_CHECK_HEADERS([syslog.h])
	
	# differ between Mac OSX and Linux
	AC_CHECK_HEADERS([features.h mach/mach_time.h sys/semaphore.h semaphore.h])
	AC_CHECK_HEADER([sys/semaphore.h], [
		CPPFLAGS="${CPPFLAGS=} -DHAVE_SYS_SEMAPHORE_H"
	])
	
	AC_C_INLINE
	AC_FUNC_ERROR_AT_LINE
	AC_FUNC_MKTIME
	AC_HEADER_STDBOOL
	
	# run type checks
	AC_TYPE_INT64_T
	AC_TYPE_SIZE_T
	AC_TYPE_UINT16_T
	AC_TYPE_UINT32_T
	AC_TYPE_UINT8_T

	dnl -----------------------------------------------
	dnl Development Options
	dnl -----------------------------------------------

	AC_MSG_CHECKING([if compiling with clang])
	AC_COMPILE_IFELSE(
	[AC_LANG_PROGRAM([], [[
	#ifndef __clang__
	       not clang
	#endif
	]])],
	[CLANG=yes], [CLANG=no])

	AC_MSG_RESULT([$CLANG])

	AC_ARG_ENABLE(gcov,
	AC_HELP_STRING([--enable-gcov], [Turn on gcov coverage testing.]),
	[
		if test "x$CLANG" = xyes; then
			AC_MSG_WARN([Code coverage is only supported in combination with GCC.])
		else
			AC_SUBST(GCOV_CFLAGS, "-fprofile-arcs -ftest-coverage -fPIC -O0")
			AC_SUBST(GCOV_LIBS, "-lgcov")
		fi
	])

	AC_ARG_ENABLE(debug,
	AC_HELP_STRING([--enable-debug], [Turn on debugging symbols.]),
	[
		CPPFLAGS="${CPPFLAGS} -ggdb -g3 -O0 -Wall -Wextra -pedantic -Wconversion -D__DEVELOPMENT_ASSERTIONS__ -pipe"
		CFLAGS="${CFLAGS} -ggdb -g3 -O0 -Wall -Wextra -pedantic -Wconversion -D__DEVELOPMENT_ASSERTIONS__ -pipe"
		AC_MSG_NOTICE([Debug symbols enabled, optimizations off])
	], [
		CPPFLAGS="${CPPFLAGS} -O3 -g -pipe"
		CFLAGS="${CFLAGS} -O3 -g -pipe"
		AC_MSG_NOTICE([Building with optimizations])
	])

	dnl -----------------------------------------------
	dnl check for netlink library
	dnl -----------------------------------------------
	AC_ARG_ENABLE([netlink],
		AS_HELP_STRING([--disable-netlink], [Build without netlink support]),
		[
			use_netlink="no"
			AC_MSG_NOTICE([Netlink support disabled])
		], [
			PKG_CHECK_MODULES([NL], [libnl-3.0], [
				REQUIRES_LIBS="$REQUIRES_LIBS libnl-3.0"
				AC_DEFINE(HAVE_LIBNL3, [1], ["netlink library 3.0 is available"])
				use_netlink="yes"

				# check for additional netlink modules
				PKG_CHECK_MODULES([NL_ROUTE], [libnl-route-3.0], [
					AC_DEFINE(HAVE_LIBNL3_ROUTE, [1], ["netlink route library 3.0 is available"])
					REQUIRES_LIBS="$REQUIRES_LIBS libnl-route-3.0"
				], [
					AC_CHECK_LIB(nl-route-3, rtnl_route_alloc, [], [
						AC_CHECK_LIB(nl-route, rtnl_route_alloc, [], [
							AC_MSG_ERROR([netlink route module not found])
						])
					])
				])

				# check for additional netlink modules
				PKG_CHECK_MODULES([NL_GENL], [libnl-genl-3.0], [
					AC_DEFINE(HAVE_LIBNL3_GENL, [1], ["netlink genl library 3.0 is available"])
					REQUIRES_LIBS="$REQUIRES_LIBS libnl-genl-3.0"
				], [
					AC_CHECK_LIB(nl-genl-3, genl_connect, [], [
						AC_CHECK_LIB(nl-genl, genl_connect, [], [
							AC_MSG_ERROR([netlink genl module not found])
						])
					])
				])
			], [
				# check for libnl-2.0
				PKG_CHECK_MODULES([NL], [libnl-2.0], [
					AC_DEFINE(HAVE_LIBNL2, [1], ["netlink library is available"])
					NL_ROUTE_LIBS="-lnl-route"
					NL_GENL_LIBS="-lnl-genl"
					REQUIRES_LIBS="$REQUIRES_LIBS libnl-2.0"
					use_netlink="yes"
				], [
					# check for legacy libnl
					PKG_CHECK_MODULES([NL], [libnl-1], [
						AC_DEFINE(HAVE_LIBNL, [1], ["netlink library is available"])
						REQUIRES_LIBS="$REQUIRES_LIBS libnl-1"
						use_netlink="yes"
					], [
						use_netlink="no"
					])
				])
			])
		]
	)

	AC_SUBST(NL_CFLAGS)
	AC_SUBST(NL_LIBS)
	AC_SUBST(NL_ROUTE_CFLAGS)
	AC_SUBST(NL_ROUTE_LIBS)
	AC_SUBST(NL_GENL_CFLAGS)
	AC_SUBST(NL_GENL_LIBS)

	dnl -----------------------------------------------
	dnl check for pthread library
	dnl -----------------------------------------------

	AC_SUBST(REQUIRES_LIBS)

	AC_ARG_WITH([mingw32-pthread],
		AS_HELP_STRING([--with-mingw32-pthread=PATH], [Build with ming32-pthread support]),
		[
			PTHREAD_LIBS="-L${withval} -lpthreadGCE2"
			PTHREAD_CFLAGS="-I${withval}"
			AC_MSG_NOTICE([using mingw32-pthread library in ${withval}])
		], [
			AC_CHECK_LIB([pthread], [main], [], AC_MSG_ERROR(POSIX threading library not found.))
			
			PTHREAD_LIBS="-lpthread"
			PTHREAD_CFLAGS=""

			AC_CHECK_FUNC(pthread_condattr_setclock,[
				AC_DEFINE(HAVE_PTHREAD_CONDATTR_SETCLOCK, [1], ["pthread clocking"])
			])
			AC_CHECK_FUNC(pthread_setconcurrency,[
				AC_DEFINE(HAVE_PTHREAD_SETCONCURRENCY, [1], ["pthread concurrency"])
			])

			AC_CHECK_FUNC(pthread_yield,[
				AC_DEFINE(HAVE_PTHREAD_YIELD, [1], ["pthread yield"],[
					AC_CHECK_FUNC(pthread_yield_np,[
						AC_DEFINE(HAVE_PTHREAD_YIELD_NP, [1], ["pthread yield np"])
					])
				])
			])

			AC_CHECK_FUNC(pthread_delay,[
				AC_DEFINE(HAVE_PTHREAD_DELAY, [1], ["pthread delay"])
			])
			AC_CHECK_FUNC(pthread_delay_np,[
				AC_DEFINE(HAVE_PTHREAD_DELAY_NP, [1], ["pthread delay np"])
			])
			AC_CHECK_FUNC(pthread_setschedprio,[
				AC_DEFINE(HAVE_PTHREAD_SETSCHEDPRIO, [1], ["pthread scheduling"])
			])
		]
	)

	AC_SUBST(PTHREAD_CFLAGS)
	AC_SUBST(PTHREAD_LIBS)

	# check for openssl
	AC_ARG_WITH(openssl,
		AS_HELP_STRING([--with-openssl], [Build with openssl support
			@<:@default=no@:>@]),
		[
		if test "x$with_openssl" = "xyes"; then
			PKG_CHECK_MODULES(OPENSSL, openssl, [
				use_openssl="yes"
				AC_DEFINE(HAVE_OPENSSL, [1], ["openssl library is available"])
				AC_DEFINE(IBRCOMMON_SUPPORT_SSL, [1], ["ssl extension is available"])
				AC_SUBST(OPENSSL_CFLAGS)
				AC_SUBST(OPENSSL_LIBS)
				REQUIRES_LIBS="$REQUIRES_LIBS openssl"
			], [
				use_openssl="no"
				AC_MSG_WARN([openssl extension requested, but no openssl library is found. ssl extensions are disabled.])
			])
		else
			use_openssl="no"
		fi
		],[
		use_openssl="no"
	])


	# Checks for lowpan support
	AC_ARG_WITH(lowpan,
		AS_HELP_STRING([--with-lowpan], [Build with lowpan support (libnl needed)
			@<:@default=no@:>@]),
	[
		AS_IF([test "x$with_lowpan" = "xyes"], [
			AS_IF([test "x$use_netlink" = "xyes"], [
				use_lowpan="yes"
				AC_DEFINE(HAVE_LOWPAN_SUPPORT, [1], ["enable lowpan support"])
				AC_DEFINE(IBRCOMMON_SUPPORT_LOWPAN, [1], ["lowpan extension is available"])
			], [
				use_lowpan="no"
				AC_MSG_WARN([lowpan extension requested, but no libnl library is found. lowpan support is disabled.])
			])
		], [
			use_lowpan="no"
		])
	],[
		use_lowpan="no"
	])

	# check for libxml
	AC_ARG_WITH(xml,
		AS_HELP_STRING([--with-xml], [Build with xml support
			@<:@default=no@:>@]),
	[
		AS_IF([test "x$with_xml" = "xyes"], [
			PKG_CHECK_MODULES(LIBXML, libxml-2.0, [
				use_xml="yes"
				AC_DEFINE(HAVE_LIBXML, [1], ["xml library is available"])
				AC_DEFINE(IBRCOMMON_SUPPORT_XML, [1], ["xml extension is available"])
				AC_SUBST(LIBXML_CFLAGS)
				AC_SUBST(LIBXML_LIBS)
				REQUIRES_LIBS="$REQUIRES_LIBS libxml-2.0"
			], [
				use_xml="no"
				AC_MSG_WARN([xml extension requested, but no xml library is found. xml extensions are disabled.])
			])
		], [
			use_xml="no"
		])
	],[
		use_xml="no"
	])
#ANDROID else end
])

# Variables for Makefile.am
AM_CONDITIONAL(OPENSSL, test x$use_openssl = xyes)
AM_CONDITIONAL(LOWPAN, test x$use_lowpan = xyes)
AM_CONDITIONAL(NETLINK, test x$use_netlink = xyes)
AM_CONDITIONAL(LIBXML, test x$use_xml = xyes)
AM_CONDITIONAL(ANDROID, test x$enable_android = xyes)
AM_CONDITIONAL(WIN32, test x$enable_win32 = xyes)


dnl -----------------------------------------------
dnl Generates Makefiles, configuration files and scripts
dnl -----------------------------------------------

AC_CONFIG_FILES([Makefile \
	ibrcommon.pc \
	ibrcommon/Makefile \
	ibrcommon/data/Makefile \
	ibrcommon/link/Makefile \
	ibrcommon/net/Makefile \
	ibrcommon/thread/Makefile \
	ibrcommon/ssl/Makefile \
	ibrcommon/ssl/gcm/Makefile \
	ibrcommon/xml/Makefile \
	tests/Makefile \
	tests/unittests/Makefile \
	tests/stress/Makefile])

AC_OUTPUT
