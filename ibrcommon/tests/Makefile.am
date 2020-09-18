## Source directory

AUTOMAKE_OPTIONS = subdir-objects
SUBDIRS=unittests stress

h_sources = \
		link/netlinktest.h \
		thread/MutexTests.h \
		thread/ThreadTest.h \
		thread/TimerTest.h \
		thread/QueueTest.h \
		net/tcpstreamtest.h \
		net/tcpclienttest.h

cc_sources = \
		link/netlinktest.cpp \
		thread/MutexTests.cpp \
		thread/ThreadTest.cpp \
		thread/TimerTest.cpp \
		thread/QueueTest.cpp \
		net/tcpstreamtest.cpp \
		net/tcpclienttest.cpp

if OPENSSL
h_sources += ssl/HashStreamTest.h \
		ssl/CipherStreamTest.h \
		ssl/RSASHA256StreamTest.h
		
cc_sources += ssl/HashStreamTest.cpp \
		ssl/CipherStreamTest.cpp \
		ssl/RSASHA256StreamTest.cpp
endif

EXTRA_DIST = base64-dec.dat base64-enc.dat test-key.pem

AM_CPPFLAGS = $(DEBUG_CFLAGS) $(OPENSSL_CFLAGS)
AM_LDFLAGS = -L@top_builddir@/ibrcommon/.libs -librcommon $(OPENSSL_LIBS)

check_PROGRAMS = testsuite
testsuite_CXXFLAGS = ${AM_CPPFLAGS} ${CPPUNIT_CFLAGS} -I@top_srcdir@/tests -I@top_srcdir@
testsuite_LDFLAGS = ${AM_LDFLAGS} ${CPPUNIT_LIBS}
testsuite_SOURCES = $(h_sources) $(cc_sources) Main.cpp

TESTS = testsuite
