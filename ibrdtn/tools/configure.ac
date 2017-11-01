dnl --------------------------------
dnl Initialization macros.
dnl --------------------------------


AC_INIT(ibrdtn-tools, m4_normalize(m4_include([version.inc])), morgenro@ibr.cs.tu-bs.de)
AC_CONFIG_SRCDIR(src/dtnping.cpp)
AM_INIT_AUTOMAKE
LT_INIT
AM_PATH_CPPUNIT(1.9.6)

AC_CONFIG_HEADER([src/config.h])
AC_CONFIG_MACRO_DIR([m4])
AM_MAINTAINER_MODE

# Test for new silent rules and enable only if they are available
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

dnl Drops autotools defaults but keeps user-specific options.
: ${CFLAGS=""}
: ${CXXFLAGS=""}

dnl -----------------------------------------------
dnl Checks for programs.
dnl -----------------------------------------------

AC_PROG_CXX
AC_PROG_CC

AC_PROG_AWK
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_LIBTOOL_DLOPEN
AC_PROG_LIBTOOL

AC_LANG_CPLUSPLUS

# Checks for libraries.
AC_CHECK_LIB([rt], [clock_gettime], [])
AC_CHECK_LIB([m], [pow], [])

# check for ibrcommon library
AC_CHECK_IBRCOMMON([1.0])

# check for ibrdtn library
AC_CHECK_IBRDTN([1.0])

# Checks for header files required by dtntunnel
AC_CHECK_HEADERS([arpa/inet.h fcntl.h sys/ioctl.h sys/socket.h])
AC_CHECK_HEADER([linux/if_tun.h], [
		has_tun="yes"
])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_TYPE_INT16_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_CHECK_FUNCS([memset])

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
		CPPFLAGS="${CPPFLAGS} -fprofile-arcs -ftest-coverage -fPIC -O0"
		CFLAGS="${CFLAGS} -fprofile-arcs -ftest-coverage -fPIC -O0"
		LIBS="${LIBS} -lgcov"
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

AC_ARG_WITH([libdaemon],
	AS_HELP_STRING([--without-libdaemon], [Build without daemonize support]),
	[
		AC_MSG_NOTICE([daemonize support disabled])
	], [
		PKG_CHECK_MODULES(DAEMON, libdaemon >= 0.12, [
			AC_SUBST(DAEMON_CFLAGS)
			AC_SUBST(DAEMON_LIBS)
			AC_DEFINE(HAVE_LIBDAEMON, [1], ["daemon library is available"])
			AC_CHECK_LIB(daemon, daemon_reset_sigs, [
				AC_DEFINE(HAVE_DAEMON_RESET_SIGS, [1], ["daemon library has daemon_reset_sigs() and daemon_unblock_sigs() functions"])
			])
		], [
			AC_MSG_WARN([daemon library not found, daemonize features disabled])
		])
	]
)

AC_ARG_WITH([libarchive],
	AS_HELP_STRING([--without-libarchive], [Build without archive support]),
	[
		has_libarchive="no"
		AC_MSG_NOTICE([archive support disabled, dtninbox and dtnoutbox will not be compiled])
	], [
		PKG_CHECK_MODULES(ARCHIVE, libarchive >= 3.0, [
			AC_SUBST(ARCHIVE_CFLAGS)
			AC_SUBST(ARCHIVE_LIBS)
			AC_DEFINE(HAVE_LIBARCHIVE, [1], ["libarchive is available"])
				has_libarchive="yes"
		], [
			AC_MSG_WARN([libarchive not found, dtninbox and dtnoutbox will not be compiled])
		])
	]
)

AC_ARG_WITH([tffs],
AS_HELP_STRING([--with-tffs=PATH], [set the tffs path for fat-image support in dtnoutbox]), [
	tffs_path=${withval}
	]
)

AS_IF([test -z "${tffs_path}"], [
	AC_MSG_WARN([tffs library path not defined. tffs support is disabled. use --with-tffs= to set path])
],[
		AC_MSG_CHECKING([for LIBTFFS])
		AS_IF([test -f "${tffs_path}/inc/tffs.h"], [
			AC_DEFINE(HAVE_LIBTFFS, [1], ["tffs found"])
			tffs_LIBS="-L${tffs_path} -ltffs"
			tffs_CFLAGS="-I${tffs_path}/inc"
			AC_SUBST(tffs_LIBS)
			AC_SUBST(tffs_CFLAGS)
			AC_MSG_RESULT([yes])
			has_tffs="yes"
		],[
				AC_MSG_RESULT([no])
		])
])

AC_ARG_WITH([bash-completion-dir],
	AS_HELP_STRING([--with-bash-completion-dir[=PATH]],
		[Install the bash auto-completion script in this directory. @<:@default=yes@:>@]),
		[],
		[with_bash_completion_dir=yes])

if test "x$with_bash_completion_dir" = "xyes"; then
	PKG_CHECK_MODULES([BASH_COMPLETION], [bash-completion >= 2.0],
	[BASH_COMPLETION_DIR="$(pkg-config --define-variable=prefix=$prefix --variable=completionsdir bash-completion)"],
	[BASH_COMPLETION_DIR="no"])
else
	BASH_COMPLETION_DIR="$with_bash_completion_dir"
fi

AC_SUBST([BASH_COMPLETION_DIR])

AM_CONDITIONAL([ENABLE_BASH_COMPLETION],[test "x$with_bash_completion_dir" != "xno"])
AM_CONDITIONAL([TUNNEL], [test x$has_tun = xyes])
AM_CONDITIONAL([LIBARCHIVE], [test x$has_libarchive = xyes])
AM_CONDITIONAL([TFFS], [test x$has_tffs = xyes])

dnl -----------------------------------------------
dnl Generates Makefiles, configuration files and scripts
dnl -----------------------------------------------

AC_CONFIG_FILES([Makefile \
	doc/Makefile \
	man/Makefile \
	src/Makefile])
	
AC_OUTPUT
