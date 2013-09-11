/*
 * Win32LinkManager.cpp
 *
 *  Created on: 11.10.2012
 *      Author: morgenro
 */

#include "ibrcommon/config.h"
#include "ibrcommon/link/Win32LinkManager.h"
#include <sstream>
#include <ws2tcpip.h>

// Link with Iphlpapi.lib
#pragma comment(lib, "IPHLPAPI.lib")

/**
 * Address change notifications:
 * http://msdn.microsoft.com/en-us/library/aa366329(v=vs.85).aspx
 */

namespace ibrcommon
{
	Win32LinkManager::Win32LinkManager() : _lm(LinkMonitor(*this))
	{
	}

	Win32LinkManager::~Win32LinkManager()
	{
		down();
	}
	void Win32LinkManager::up() throw()
	{
		_lm.start();
	}

	void Win32LinkManager::down() throw()
	{
		_lm.stop();
		_lm.join();
	}

	void Win32LinkManager::freeAdapterInfo(IP_ADAPTER_ADDRESSES *pAddresses) const
	{
		HeapFree(GetProcessHeap(), 0, (pAddresses));
	}

	IP_ADAPTER_ADDRESSES* Win32LinkManager::getAdapterInfo() const
	{
		PIP_ADAPTER_ADDRESSES pAddresses = NULL;

		// Set the flags to pass to GetAdaptersAddresses
		ULONG flags = GAA_FLAG_INCLUDE_PREFIX;

		// default to unspecified address family (both)
		ULONG family = AF_UNSPEC;

		DWORD dwSize = 0;
		DWORD dwRetVal = 0;

		// Allocate a 15 KB buffer to start with.
		ULONG outBufLen = 15000;

		for (int i = 0; i < 3; i++)
		{
			pAddresses = (IP_ADAPTER_ADDRESSES *) HeapAlloc(GetProcessHeap(), 0, (outBufLen));
			if (pAddresses == NULL) {
				throw ibrcommon::Exception("Memory allocation failed for IP_ADAPTER_ADDRESSES struct");
			}

			dwRetVal = GetAdaptersAddresses(family, flags, NULL, pAddresses, &outBufLen);

			if (dwRetVal == ERROR_BUFFER_OVERFLOW) {
				freeAdapterInfo(pAddresses);
				pAddresses = NULL;
			} else {
				break;
			}
		}

		if (dwRetVal == NO_ERROR) return pAddresses;

		if (pAddresses) freeAdapterInfo(pAddresses);

		if (dwRetVal == ERROR_NO_DATA) {
			throw ibrcommon::Exception("No addresses were found for the requested parameters");
		} else {
			std::stringstream ss;
			ss << "Call to GetAdaptersAddresses failed with error: " << dwRetVal;
			throw ibrcommon::Exception(ss.str());
		}
	}

	const vinterface Win32LinkManager::getInterface(int index) const
	{
		vinterface ret;

		PIP_ADAPTER_ADDRESSES pAddresses = getAdapterInfo();

		// find the interface for the given index
		for (PIP_ADAPTER_ADDRESSES pCurrAddresses = pAddresses; pCurrAddresses->Next != NULL; pCurrAddresses = pCurrAddresses->Next)
		{
			if (pCurrAddresses->IfIndex == index) {
				ret = vinterface(pCurrAddresses->AdapterName);
				break;
			}
		}

		freeAdapterInfo(pAddresses);
		return ret;
	}

	vaddress Win32LinkManager::getAddress(SOCKET_ADDRESS &sockaddr) const {
		char str_address[256];
		DWORD str_length = static_cast<DWORD>(sizeof str_address);
		if (WSAAddressToString(sockaddr.lpSockaddr, sockaddr.iSockaddrLength, NULL, str_address, &str_length) == 0) {
			return vaddress(std::string(str_address), "");
		}

		throw ibrcommon::Exception("can not convert address into a string");
	}

	const std::list<vaddress> Win32LinkManager::getAddressList(const vinterface &iface, const std::string &scope)
	{
		std::list<vaddress> ret;

		PIP_ADAPTER_ADDRESSES pAddresses = getAdapterInfo();

		for (PIP_ADAPTER_ADDRESSES pCurrAddresses = pAddresses; pCurrAddresses->Next != NULL; pCurrAddresses = pCurrAddresses->Next)
		{
			const std::string name(pCurrAddresses->AdapterName);

			if (iface.toString() == name) {
				PIP_ADAPTER_UNICAST_ADDRESS pUnicast;

				for (pUnicast = pCurrAddresses->FirstUnicastAddress; pUnicast != NULL; pUnicast = pUnicast->Next) {
					// skip transient addresses
					if (pUnicast->Flags & IP_ADAPTER_ADDRESS_TRANSIENT) continue;

					if (scope.length() > 0) {
						// scope filter only available with IPv6
						if (pUnicast->Address.lpSockaddr->sa_family == AF_INET6) {
							// get ipv6 specific address
							sockaddr_in6 *addr6 = (sockaddr_in6*)pUnicast->Address.lpSockaddr;

							// if the id is set, then this scope is link-local
							if (addr6->sin6_scope_id == 0) {
								if (scope != vaddress::SCOPE_GLOBAL) continue;
							} else {
								if (scope != vaddress::SCOPE_LINKLOCAL) continue;
							}
						}
					}

					// cast to a sockaddr
					ret.push_back( getAddress(pUnicast->Address) );
				}

				break;
			}
		}

		freeAdapterInfo(pAddresses);

		// return the list of addresses bound to given interface
		return ret;
	}

	std::set<ibrcommon::vinterface> Win32LinkManager::getInterfaces() const
	{
		std::set<ibrcommon::vinterface> ret;

		PIP_ADAPTER_ADDRESSES pAddresses = getAdapterInfo();

		for (PIP_ADAPTER_ADDRESSES pCurrAddresses = pAddresses; pCurrAddresses->Next != NULL; pCurrAddresses = pCurrAddresses->Next)
		{
			const std::string name(pCurrAddresses->AdapterName);
			const std::wstring friendlyName(pCurrAddresses->FriendlyName);

			ibrcommon::vinterface iface(name, friendlyName);
			ret.insert(iface);
		}

		freeAdapterInfo(pAddresses);

		return ret;
	}

} /* namespace ibrcommon */
