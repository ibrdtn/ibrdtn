## sub directory

storage_SOURCES = \
	BundleStorage.cpp \
	BundleStorage.h \
	MemoryBundleStorage.h \
	MemoryBundleStorage.cpp \
	SimpleBundleStorage.cpp \
	SimpleBundleStorage.h \
	DataStorage.h \
	DataStorage.cpp \
	BundleResult.h \
	BundleResult.cpp \
	BundleIndex.h \
	BundleIndex.cpp \
	BundleSeeker.h \
	BundleSelector.h \
	MetaStorage.h \
	MetaStorage.cpp
	

if SQLITE
storage_SOURCES += SQLiteConfigure.h SQLiteConfigure.cpp SQLiteBundleStorage.h SQLiteBundleStorage.cpp SQLiteDatabase.h SQLiteDatabase.cpp SQLiteBundleSet.h SQLiteBundleSet.cpp SQLiteBundleSetFactory.h SQLiteBundleSetFactory.cpp
endif

AM_CPPFLAGS = -I$(top_srcdir)/src $(ibrdtn_CFLAGS) $(SQLITE_CFLAGS)
AM_LDFLAGS = $(ibrdtn_LIBS) $(SQLITE_LIBS)

if ANDROID
noinst_DATA = Android.mk
CLEANFILES = Android.mk
else
noinst_LTLIBRARIES = libstorage.la
libstorage_la_SOURCES= $(storage_SOURCES)
endif

Android.mk: Makefile.am
	$(ANDROGENIZER) -:PROJECT dtnd \
		-:REL_TOP $(top_srcdir) -:ABS_TOP $(abs_top_srcdir) \
		-:STATIC libdtnd_storage \
		-:SOURCES $(storage_SOURCES) \
		-:CPPFLAGS $(CPPFLAGS) $(AM_CPPFLAGS) \
		-:LDFLAGS $(AM_LDFLAGS) \
		> $@
