
noinst_HEADERS = \
	BaseRouterTest.hh \
	BundleStorageTest.hh \
	BundleSetTest.hh \
	ConfigurationTest.hh \
	DaemonTest.hh \
	DatagramClTest.h \
	DataStorageTest.h \
	FakeDatagramService.h \
	NativeSerializerTest.h \
	NodeTest.hh

unittest_SOURCES = \
	Main.cpp \
	BaseRouterTest.cpp \
	BundleStorageTest.cpp \
	BundleSetTest.cpp \
	ConfigurationTest.cpp \
	DaemonTest.cpp \
	DatagramClTest.cpp \
	DataStorageTest.cpp \
	FakeDatagramService.cpp \
	NativeSerializerTest.cpp \
	NodeTest.cpp

# what flags you want to pass to the C compiler & linker
AM_CPPFLAGS = $(ibrdtn_CFLAGS) $(CPPUNIT_CFLAGS) $(CURL_CFLAGS) $(SQLITE_CFLAGS)
AM_LDFLAGS = $(ibrdtn_LIBS) $(CPPUNIT_LIBS) $(CURL_LIBS) $(SQLITE_LIBS)

check_PROGRAMS = unittest
unittest_CXXFLAGS = $(AM_CPPFLAGS) $(CPPUNIT_CFLAGS) -I$(top_srcdir)/tests/unittests -I$(top_srcdir)/src
unittest_LDFLAGS = $(AM_LDFLAGS) $(CPPUNIT_LIBS)
unittest_LDADD = $(top_srcdir)/src/libdtnd.la

TESTS = unittest
