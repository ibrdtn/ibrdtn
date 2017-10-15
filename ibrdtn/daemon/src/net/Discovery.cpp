#include "net/Discovery.h"
#include <arpa/inet.h>

namespace dtn {
	namespace net {
		namespace ipnd {

			template<> const std::string tagString<BOOLEAN   >() { return "BOOLEAN"   ; }
			template<> const std::string tagString<UINT64    >() { return "UINT64"    ; }
			template<> const std::string tagString<SINT64    >() { return "SINT64"    ; }
			template<> const std::string tagString<FIXED16   >() { return "FIXED16"   ; }
			template<> const std::string tagString<FIXED32   >() { return "FIXED32"   ; }
			template<> const std::string tagString<FIXED64   >() { return "FIXED64"   ; }
			template<> const std::string tagString<FLOAT     >() { return "FLOAT"     ; }
			template<> const std::string tagString<DOUBLE    >() { return "DOUBLE"    ; }
			template<> const std::string tagString<STRING    >() { return "STRING"    ; }
			template<> const std::string tagString<BYTES     >() { return "BYTES"     ; }
			template<> const std::string tagString<CLA_TCP_v4>() { return "CLA_TCP_v4"; }
			template<> const std::string tagString<CLA_UDP_v4>() { return "CLA_UDP_v4"; }
			template<> const std::string tagString<CLA_TCP_v6>() { return "CLA_TCP_v6"; }
			template<> const std::string tagString<CLA_UDP_v6>() { return "CLA_UDP_v6"; }
			template<> const std::string tagString<CLA_DGRAM_UDP     >() { return "CLA_DGRAM_UDP"     ; }
			template<> const std::string tagString<CLA_DGRAM_ETHERNET>() { return "CLA_DGRAM_ETHERNET"; }
			template<> const std::string tagString<CLA_DGRAM_LOWPAN  >() { return "CLA_DGRAM_LOWPAN"  ; }
			template<> const std::string tagString<CLA_LOWPAN>() { return "CLA_LOWPAN"; }
			template<> const std::string tagString<CLA_EMAIL >() { return "CLA_EMAIL" ; }
			template<> const std::string tagString<CLA_DHT   >() { return "CLA_DHT"   ; }
			template<> const std::string tagString<CLA_DTNTP >() { return "CLA_DTNTP" ; }

		} // namespace ipnd
	} // namespace net
} // namespace dtn
