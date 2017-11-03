## sub directory

h_sources = \
	nl802154.h \
	LinkManager.h \
	LinkEvent.h

cc_sources = \
	LinkManager.cpp \
	LinkEvent.cpp

if LOWPAN
cc_sources += nl_policy.c
endif

h_sources += \
	CompatLinkManager.h \
	LinkMonitor.h

cc_sources += \
	CompatLinkManager.cpp \
	LinkMonitor.cpp

if WIN32
h_sources += Win32LinkManager.h
cc_sources += Win32LinkManager.cpp
else
if NETLINK
h_sources += NetLinkManager.h
cc_sources += NetLinkManager.cpp
endif
h_sources += PosixLinkManager.h
cc_sources += PosixLinkManager.cpp
endif

#Install the headers in a versioned directory
library_includedir = $(includedir)/$(GENERIC_LIBRARY_NAME)-$(GENERIC_API_VERSION)/$(GENERIC_LIBRARY_NAME)/link
library_include_HEADERS = $(h_sources)

AUTOMAKE_OPTIONS = nostdinc
AM_CPPFLAGS = -I$(top_srcdir) $(PTHREAD_CFLAGS) $(NL_CFLAGS) $(NL_ROUTE_CFLAGS) $(NL_GENL_CFLAGS) $(GCOV_CFLAGS)
AM_LDFLAGS = $(PTHREAD_LIBS) $(NL_LIBS) $(NL_ROUTE_LIBS) $(NL_GENL_LIBS) $(GCOV_LIBS)

if ANDROID
noinst_DATA = Android.mk
CLEANFILES = Android.mk
else
noinst_LTLIBRARIES = libibrcommon_link.la
libibrcommon_link_la_SOURCES = $(h_sources) $(cc_sources)
endif

Android.mk: Makefile.am
	$(ANDROGENIZER) -:PROJECT $(GENERIC_LIBRARY_NAME) \
		-:REL_TOP $(top_srcdir) -:ABS_TOP $(abs_top_srcdir) \
		-:STATIC libibrcommon_link \
		-:SOURCES $(h_sources) $(cc_sources) \
		-:CPPFLAGS $(AM_CPPFLAGS) \
		-:LDFLAGS $(AM_LDFLAGS) \
		-:LIBFILTER_STATIC nl-3 nl-genl-3 nl-route-3 \
		> $@
