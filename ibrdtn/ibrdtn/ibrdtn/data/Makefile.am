## sub directory

h_sources = \
	Serializer.h \
	AgeBlock.h \
	ScopeControlHopLimitBlock.h \
	Block.h \
	BlobBlock.h \
	Bundle.h \
	BundleID.h \
	BundleList.h \
	BundleSet.h \
	BundleMerger.h \
	BundleString.h \
	CustodySignalBlock.h \
	Dictionary.h \
	DTNTime.h \
	EID.h \
	Exceptions.h \
	ExtensionBlock.h \
	MetaBundle.h \
	PayloadBlock.h \
	PrimaryBlock.h \
	SDNV.h \
	StatusReportBlock.h \
	BundleFragment.h \
	StreamBlock.h \
	BundleBuilder.h \
	AdministrativeBlock.h \
	TrackingBlock.h \
	SchedulingBlock.h \
	Number.h \
	MemoryBundleSet.h \
	BundleSetImpl.h \
	Endianess.h \
	Endianess.cpp

cc_sources = \
	Serializer.cpp \
	AgeBlock.cpp \
	ScopeControlHopLimitBlock.cpp \
	Block.cpp \
	Bundle.cpp \
	BundleID.cpp \
	BundleList.cpp \
	BundleSet.cpp \
	BundleMerger.cpp \
	BundleString.cpp \
	CustodySignalBlock.cpp \
	Dictionary.cpp \
	DTNTime.cpp \
	EID.cpp \
	ExtensionBlock.cpp \
	MetaBundle.cpp \
	PayloadBlock.cpp \
	PrimaryBlock.cpp \
	SDNV.cpp \
	StatusReportBlock.cpp \
	BundleFragment.cpp \
	StreamBlock.cpp \
	BundleBuilder.cpp \
	AdministrativeBlock.cpp \
	TrackingBlock.cpp \
	SchedulingBlock.cpp \
	MemoryBundleSet.cpp \
	BundleSetImpl.cpp

if COMPRESSION
h_sources += CompressedPayloadBlock.h
cc_sources += CompressedPayloadBlock.cpp
endif

#Install the headers in a versioned directory
library_includedir = $(includedir)/$(GENERIC_LIBRARY_NAME)-$(GENERIC_API_VERSION)/$(GENERIC_LIBRARY_NAME)/data
library_include_HEADERS = $(h_sources)

AM_CPPFLAGS = -I$(top_srcdir) $(ibrcommon_CFLAGS) $(GLIB_CFLAGS) $(GCOV_CFLAGS)
AM_LDFLAGS = $(ibrcommon_LIBS) $(GLIB_LIBS) $(GCOV_LIBS)

if ANDROID
noinst_DATA = Android.mk
CLEANFILES = Android.mk
else
noinst_LTLIBRARIES = libdata.la
libdata_la_SOURCES = $(h_sources) $(cc_sources)
endif

Android.mk: Makefile.am
	$(ANDROGENIZER) -:PROJECT $(GENERIC_LIBRARY_NAME) \
		-:REL_TOP $(top_srcdir) -:ABS_TOP $(abs_top_srcdir) \
		-:STATIC libibrdtn_data \
		-:SOURCES $(h_sources) $(cc_sources) \
		-:CPPFLAGS $(AM_CPPFLAGS) \
		-:LDFLAGS $(AM_LDFLAGS) \
		> $@
