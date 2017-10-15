## sub directory

net_SOURCES = 	BundleReceiver.h \
	BundleTransfer.h \
	BundleTransfer.cpp \
	ConnectionEvent.cpp \
	ConnectionEvent.h \
	ConnectionManager.cpp \
	ConnectionManager.h \
	ConvergenceLayer.cpp \
	ConvergenceLayer.h \
	Discovery.cpp \
	Discovery.h \
	DiscoveryAgent.cpp \
	DiscoveryAgent.h \
	DiscoveryBeacon.cpp \
	DiscoveryBeacon.h \
	DiscoveryService.cpp \
	DiscoveryService.h \
	DiscoveryServiceParam.cpp \
	DiscoveryServiceParam.h \
	DiscoveryBeaconHandler.cpp \
	DiscoveryBeaconHandler.h \
	IPNDAgent.cpp \
	IPNDAgent.h \
	TCPConnection.cpp \
	TCPConnection.h \
	TCPConvergenceLayer.cpp \
	TCPConvergenceLayer.h \
	TransferAbortedEvent.cpp \
	TransferAbortedEvent.h \
	TransferCompletedEvent.cpp \
	TransferCompletedEvent.h \
	UDPConvergenceLayer.cpp \
	UDPConvergenceLayer.h \
	FileConvergenceLayer.cpp \
	FileConvergenceLayer.h \
	DatagramConvergenceLayer.h \
	DatagramConnection.h \
	DatagramConvergenceLayer.cpp \
	DatagramConnection.cpp \
	DatagramService.h \
	DatagramService.cpp \
	UDPDatagramService.h \
	UDPDatagramService.cpp \
	FileMonitor.h \
	FileMonitor.cpp \
	P2PDialupExtension.h \
	P2PDialupExtension.cpp \
	P2PDialupEvent.h \
	P2PDialupEvent.cpp \
	NativeP2pManager.h \
	NativeP2pManager.cpp

if LOWPAN
net_SOURCES += LOWPANConvergenceLayer.cpp LOWPANConvergenceLayer.h LOWPANConnection.cpp LOWPANConnection.h
net_SOURCES += LOWPANDatagramService.cpp LOWPANDatagramService.h
endif
				
if CURL
net_SOURCES += HTTPConvergenceLayer.cpp HTTPConvergenceLayer.h
endif

if DTNDHT
net_SOURCES += DHTNameService.cpp DHTNameService.h
endif

if WIFIP2P
net_SOURCES += WifiP2PManager.h WifiP2PManager.cpp
endif

if VMIME
net_SOURCES += EMailConvergenceLayer.cpp EMailConvergenceLayer.h
net_SOURCES += EMailSmtpService.cpp EMailSmtpService.h EMailImapService.cpp EMailImapService.h
endif

AM_CPPFLAGS = -I$(top_srcdir)/src $(ibrdtn_CFLAGS) $(CURL_CFLAGS) $(DTNDHT_CFLAGS) $(WIFIP2P_CFLAGS) $(VMIME_CFLAGS) $(GCOV_CFLAGS)
AM_LDFLAGS = $(ibrdtn_LIBS) $(CURL_LIBS) $(DTNDHT_LIBS) $(WIFIP2P_LIBS) $(VMIME_LIBS) $(GCOV_LIBS)

if ANDROID
noinst_DATA = Android.mk
CLEANFILES = Android.mk
else
noinst_LTLIBRARIES = libnet.la
libnet_la_SOURCES= $(net_SOURCES)
endif

Android.mk: Makefile.am
	$(ANDROGENIZER) -:PROJECT dtnd \
		-:REL_TOP $(top_srcdir) -:ABS_TOP $(abs_top_srcdir) \
		-:STATIC libdtnd_net \
		-:SOURCES $(net_SOURCES) \
		-:CPPFLAGS $(CPPFLAGS) $(AM_CPPFLAGS) \
		-:LDFLAGS $(AM_LDFLAGS) \
		> $@
