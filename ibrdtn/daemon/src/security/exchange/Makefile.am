## sub directory

exchange_SOURCES =

if DTNSEC
exchange_SOURCES += \
	DHProtocol.h \
	DHProtocol.cpp \
	HashProtocol.h \
	HashProtocol.cpp \
	KeyExchangeData.h \
	KeyExchangeData.cpp \
	KeyExchangeEvent.h \
	KeyExchangeEvent.cpp \
	KeyExchangeProtocol.h \
	KeyExchangeProtocol.cpp \
	KeyExchanger.h \
	KeyExchanger.cpp \
	KeyExchangeSession.h \
	KeyExchangeSession.cpp \
	NFCProtocol.h \
	NFCProtocol.cpp \
	NoneProtocol.h \
	NoneProtocol.cpp \
	openssl_compat.h \
	openssl_compat.cpp \
	QRCodeProtocol.h \
	QRCodeProtocol.cpp
	
if OPENSSL_JPAKE
exchange_SOURCES += \
	JPAKEProtocol.h \
	JPAKEProtocol.cpp
endif
endif

AM_CPPFLAGS = -I$(top_srcdir)/src $(ibrdtn_CFLAGS) $(OPENSSL_CFLAGS)
AM_LDFLAGS = $(ibrdtn_LIBS) $(OPENSSL_LIBS)

if ANDROID
noinst_DATA = Android.mk
CLEANFILES = Android.mk
else
noinst_LTLIBRARIES = libsecexchange.la
libsecexchange_la_SOURCES= $(exchange_SOURCES)
endif

Android.mk: Makefile.am
	$(ANDROGENIZER) -:PROJECT dtnd \
		-:REL_TOP $(top_srcdir) -:ABS_TOP $(abs_top_srcdir) \
		-:STATIC libdtnd_secexchange \
		-:SOURCES $(exchange_SOURCES) \
		-:CPPFLAGS $(CPPFLAGS) $(AM_CPPFLAGS) \
		-:LDFLAGS $(AM_LDFLAGS) \
		> $@
