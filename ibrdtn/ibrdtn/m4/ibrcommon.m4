AC_DEFUN([AC_CHECK_IBRCOMMON],
[
	AC_MSG_CHECKING([checking for ibrcommon >= $1])
	ibrcommon=notfound

	AC_ARG_WITH([ibrcommon],
		AS_HELP_STRING([--with-ibrcommon=PATH], [set the ibrcommon source path]), [
			ibrcommon_path=${withval}
		]
	)
	
	AS_IF([test -z "${ibrcommon_path}" -a -e "$(pwd)/../ibrcommon/ibrcommon.pc"], [
		# set relative path
		ibrcommon_path="$(pwd)/../ibrcommon"
	])
	
	AS_IF([test -n "${ibrcommon_path}"], [
		# allow the pattern PKG_CONFIG_PATH
		m4_pattern_allow([^PKG_CONFIG_PATH$])
		
		# export the path of ibrcommon
		export PKG_CONFIG_PATH="${PKG_CONFIG_PATH}:${ibrcommon_path}"
		
		# check for the svn version of ibrcommon
		if pkg-config --atleast-version=$1 ibrcommon; then
			# read LIBS options for ibrcommon
			ibrcommon_LIBS="-L${ibrcommon_path}/ibrcommon/.libs $(pkg-config --libs ibrcommon)"
			
			# read CFLAGS options for ibrcommon
			ibrcommon_CFLAGS="-I${ibrcommon_path} $(pkg-config --cflags ibrcommon)"
			
			# set ibrcommon as available
			ibrcommon=yes
			AC_MSG_RESULT([yes])
		fi
	])
	
	AS_IF([test "x${ibrcommon}" = "xnotfound"], [
		# check for ibrcommon library
		PKG_CHECK_MODULES([ibrcommon], [ibrcommon >= $1], [
			AC_MSG_RESULT([yes])
		], [
			AC_MSG_RESULT([no])
			AC_MSG_ERROR([ibrcommon library not found!])
		])
	])
	
	AC_SUBST(ibrcommon_CFLAGS)
	AC_SUBST(ibrcommon_LIBS)
])

AC_DEFUN([AC_CHECK_IBRCOMMON_SSL],
[
	AC_MSG_CHECKING([checking whether ibrcommon has SSL extensions])
	old_CPPFLAGS="$CPPFLAGS"
	CPPFLAGS="$ibrcommon_CFLAGS"
	AC_COMPILE_IFELSE([
		AC_LANG_PROGRAM([[
			#include <ibrcommon/ibrcommon.h>
		]], [[
			#ifdef IBRCOMMON_SUPPORT_SSL
			// SSL is supported
			#else
			# SSL not supported
			#endif
		]])
	], [
		AC_MSG_RESULT([yes])
		$1
	], [
		AC_MSG_RESULT([no])
		$2
	])
	CPPFLAGS="$old_CPPFLAGS"
])

AC_DEFUN([AC_CHECK_IBRCOMMON_LOWPAN],
[
	AC_MSG_CHECKING([checking whether ibrcommon has lowpan extensions])
	old_CPPFLAGS="$CPPFLAGS"
	CPPFLAGS="$ibrcommon_CFLAGS"
	AC_COMPILE_IFELSE([
		AC_LANG_PROGRAM([[
			#include <ibrcommon/ibrcommon.h>
		]], [[
			#ifdef IBRCOMMON_SUPPORT_LOWPAN
			// LOWPAN is supported
			#else
			# LOWPAN not supported
			#endif
		]])
	], [
		AC_MSG_RESULT([yes])
		$1
	], [
		AC_MSG_RESULT([no])
		$2
	])
	CPPFLAGS="$old_CPPFLAGS"
])

AC_DEFUN([AC_CHECK_IBRCOMMON_XML],
[
	AC_MSG_CHECKING([checking whether ibrcommon has XML extensions])
	old_CPPFLAGS="$CPPFLAGS"
	CPPFLAGS="$ibrcommon_CFLAGS"
	AC_COMPILE_IFELSE([
		AC_LANG_PROGRAM([[
			#include <ibrcommon/ibrcommon.h>
		]], [[
			#ifdef IBRCOMMON_SUPPORT_XML
			// XML is supported
			#else
			# XML not supported
			#endif
		]])
	], [
		AC_MSG_RESULT([yes])
		$1
	], [
		AC_MSG_RESULT([no])
		$2
	])
	CPPFLAGS="$old_CPPFLAGS"
])
