## Source directory
AUTOMAKE_OPTIONS = foreign

SUBDIRS = unittests

h_sources = tools/EventSwitchLoop.h tools/TestEventListener.h
cc_sources = 

# what flags you want to pass to the C compiler & linker
AM_CPPFLAGS = $(ibrdtn_CFLAGS) $(CPPUNIT_CFLAGS) $(CURL_CFLAGS) $(SQLITE_CFLAGS) $(DTNDHT_CFLAGS)
AM_LDFLAGS = $(ibrdtn_LIBS) $(CPPUNIT_LIBS) $(CURL_LIBS) $(SQLITE_LIBS) $(DTNDHT_LIBS)

check_PROGRAMS = testsuite
testsuite_CXXFLAGS = -I$(top_srcdir)/src
testsuite_LDADD = $(top_srcdir)/src/libdtnd.la
testsuite_SOURCES = $(h_sources) $(cc_sources) testsuite.cpp

TESTS = testsuite
