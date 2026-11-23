## Source directory

AUTOMAKE_OPTIONS = subdir-objects
dist_noinst_DATA = test-key.pem

h_sources = data/TestSDNV.h data/TestEID.h data/TestBundleList.h data/TestBundleSet.h data/TestDictionary.h data/TestSerializer.h net/TestStreamConnection.h api/TestPlainSerializer.h utils/TestUtils.h data/TestExtensionBlock.h data/TestTrackingBlock.h data/TestBundleString.h data/TestBundleID.h
cc_sources = data/TestSDNV.cpp data/TestEID.cpp data/TestBundleList.cpp data/TestBundleSet.cpp data/TestDictionary.cpp data/TestSerializer.cpp net/TestStreamConnection.cpp api/TestPlainSerializer.cpp utils/TestUtils.cpp data/TestExtensionBlock.cpp data/TestTrackingBlock.cpp data/TestBundleString.cpp data/TestBundleID.cpp Main.cpp

if DTNSEC
h_sources += security/TestSecurityBlock.h security/PayloadConfidentialBlockTest.h security/PayloadIntegrityBlockTest.h
cc_sources += security/TestSecurityBlock.cpp security/PayloadConfidentialBlockTest.cpp security/PayloadIntegrityBlockTest.cpp
endif

if COMPRESSION
h_sources += data/TestCompressedPayloadBlock.h
cc_sources += data/TestCompressedPayloadBlock.cpp
endif

AM_CPPFLAGS = $(PTHREAD_CFLAGS) $(ibrcommon_CFLAGS) $(ZLIB_CFLAGS)
AM_LDFLAGS = $(PTHREAD_LIBS) $(ibrcommon_LIBS) -L@top_builddir@/ibrdtn/.libs -librdtn $(ZLIB_LIBS)

AM_CPPFLAGS += -I@top_srcdir@

check_PROGRAMS = testsuite
testsuite_CXXFLAGS = ${AM_CPPFLAGS} ${CPPUNIT_CFLAGS} -I@top_srcdir@/tests
testsuite_LDADD = ${AM_LDFLAGS} ${CPPUNIT_LIBS}
testsuite_SOURCES = $(h_sources) $(cc_sources)

TESTS = testsuite
