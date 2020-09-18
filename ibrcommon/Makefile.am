## top directory

#Build in these directories:
AUTOMAKE_OPTIONS = subdir-objects
SUBDIRS = $(GENERIC_LIBRARY_NAME)

#Distribute these directories:
DIST_SUBDIRS = $(GENERIC_LIBRARY_NAME)

if ANDROID
noinst_DATA = Android.mk
CLEANFILES = Android.mk
else
SUBDIRS += tests
DIST_SUBDIRS += tests
endif

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = $(GENERIC_LIBRARY_NAME).pc

# debian files for package building
EXTRA_DIST = \
	debian/compat \
	debian/ibrcommon1.install \
	debian/copyright \
	debian/changelog \
	debian/rules \
	debian/ibrcommon1.dirs \
	debian/control \
	debian/docs \
	debian/source \
	debian/source/format \
	debian/ibrcommon-dev.install \
	debian/ibrcommon-dev.dirs

#EXTRA_DIST=autogen.sh

ACLOCAL_AMFLAGS = -I m4

Android.mk: Makefile.am
	$(ANDROGENIZER) -:PROJECT $(GENERIC_LIBRARY_NAME) \
		-:REL_TOP $(top_srcdir) -:ABS_TOP $(abs_top_srcdir) \
		-:SUBDIR $(SUBDIRS) \
		> $@
