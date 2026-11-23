
noinst_HEADERS = \
	Base64StreamTest.h \
	BLOBTest.hh \
	BloomFilterTest.hh \
	FileTest.hh \
	iobufferTest.h \
	IteratorTest.h \
	refcnt_ptrTest.hh \
	stopandwaitTest.hh

unittest_SOURCES = \
	Main.cpp \
	Base64StreamTest.cpp \
	BLOBTest.cpp \
	BloomFilterTest.cpp \
	FileTest.cpp \
	iobufferTest.cpp \
	IteratorTest.cpp \
	refcnt_ptrTest.cpp \
	stopandwaitTest.cpp

AM_CPPFLAGS = $(DEBUG_CFLAGS)
AM_LDFLAGS = -L@top_builddir@/ibrcommon/.libs -librcommon

check_PROGRAMS = unittest
unittest_CXXFLAGS = ${AM_CPPFLAGS} ${CPPUNIT_CFLAGS} -I@top_srcdir@/tests/unittests -I@top_srcdir@
unittest_LDADD = ${AM_LDFLAGS} ${CPPUNIT_LIBS}

TESTS = unittest
