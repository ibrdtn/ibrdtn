set(ibrcommon_sources
    appstreambuf.cpp
    Logger.cpp
    MonotonicClock.cpp
    TimeMeasurement.cpp
    data/Base64.cpp
    data/Base64Reader.cpp
    data/Base64Stream.cpp
    data/BLOB.cpp
    data/BloomFilter.cpp
    data/ConfigFile.cpp
    data/File.cpp
    data/iobuffer.cpp
    link/CompatLinkManager.cpp
    link/LinkEvent.cpp
    link/LinkManager.cpp
    link/LinkMonitor.cpp
    net/socket.cpp
    net/socketstream.cpp
    net/stopandwait.cpp
    net/vaddress.cpp
    net/vinterface.cpp
    net/vsocket.cpp
    thread/AtomicCounter.cpp
    thread/Conditional.cpp
    thread/Mutex.cpp
    thread/MutexLock.cpp
    thread/RWLock.cpp
    thread/RWMutex.cpp
    thread/Semaphore.cpp
    thread/SignalHandler.cpp
    thread/Thread.cpp
    thread/Timer.cpp
)

set(sslsupport_sources
    ssl/AES128Stream.cpp
    ssl/CipherStream.cpp
    ssl/HashStream.cpp
    ssl/HMacStream.cpp
    ssl/InputCipherStream.cpp
    ssl/iostreamBIO.cpp
    ssl/MD5Stream.cpp
    ssl/RSASHA256Stream.cpp
    ssl/SHA256Stream.cpp
    ssl/TLSStream.cpp
    ssl/XORStream.cpp
    ssl/gcm/gcm.cpp
    ssl/gcm/gf128mul.cpp
)

# Choose an implementation for netlink manager.
option(Netlink "Build with netlink library [default=ON]" ON)

if (WIN32)
  list(APPEND ibrcommon_sources link/Win32LinkManager.cpp)
else()
  if (Netlink)
    pkg_search_module(PC_LIBNL libnl-3.0)
    if(PKG_CONFIG_FOUND)
      add_definitions(-DHAVE_LIBNL3=1)
      set(HAS_NETLINK 1)
    else()
      pkg_search_module(PC_LIBNL libnl-2.0)
      if(PKG_CONFIG_FOUND)
        add_definitions(-DHAVE_LIBNL2=1)
        set(HAS_NETLINK 1)
      else()
        pkg_search_module(PC_LIBNL libnl-1)
        if(PKG_CONFIG_FOUND)
          add_definitions(-DHAVE_LIBNL=1)
          set(HAS_NETLINK 1)
        endif()
      endif()
    endif()
  endif ()
  if (HAS_NETLINK)
    message(STATUS "Building with libnl-" ${PC_LIBNL_VERSION})
    include_directories("${PC_LIBNL_INCLUDE_DIRS}")
    list(APPEND ibrcommon_sources link/NetLinkManager.cpp)
  else()
    list(APPEND ibrcommon_sources link/PosixLinkManager.cpp)
  endif()
endif()

# Enable/disable lowpan.
option(Lowpan "Build with lowpan support [default=OFF]" OFF)

if (Lowpan)
  message(STATUS "Building with lowpan")
  if (NOT(HAS_NETLINK))
    message(SEND_ERROR "Netlink is required for lowpan support")
  endif()
  list(APPEND ibrcommon_sources net/lowpansocket.cpp net/lowpanstream.cpp)
endif()

# Enable/disable openssl
option(Openssl "Build with openssl support [default=OFF]" OFF)

if (Openssl)
  pkg_search_module(OPENSSL REQUIRED openssl)
  message(STATUS "Building with openssl-" ${OPENSSL_VERSION})
  list(APPEND ibrcommon_sources ${sslsupport_sources})
endif()

# Enable/disable xml
option(Xml "Build with xml support [default=OFF]" OFF)

if (Xml)
  pkg_search_module(LIBXML2 REQUIRED libxml-2.0)
  message(STATUS "Building with libxml-" ${LIBXML2_VERSION})
  list(APPEND ibrcommon_sources xml/XMLStreamReader.cpp xml/XMLStreamWriter.cpp)
  include_directories("${LIBXML2_INCLUDE_DIRS}")
endif()

# TODO: implement and test HAVE_FEATURES_H

# TODO: implement and test HAVE_MACH_MACH_TIME_H

# TODO: implement and test HAVE_SYSLOG_H
#  list(APPEND ibrcommon_sources SyslogStream.cpp)

# TODO: find out where CLOCK_BOOTTIME can come from.
# TODO: find out where MCAST_JOIN_GROUP can come from.
# TODO: find out where MCAST_LEAVE_GROUP can come from.

# Generate a dummy config.h not used with CMake.
configure_file(config.h.cmake ibrcommon/config.h COPYONLY)
include_directories(${CMAKE_CURRENT_BINARY_DIR})

# build ibrcommon as static lib.
add_library(ibrcommon MODULE ${ibrcommon_sources})
