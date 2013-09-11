/*
 * Win32LinkManager.h
 *
 *  Created on: 11.10.2012
 *      Author: morgenro
 */

#ifndef DEFAULTLINKMANAGER_H_
#define DEFAULTLINKMANAGER_H_

#include "ibrcommon/link/LinkManager.h"
#include "ibrcommon/net/vinterface.h"
#include "ibrcommon/net/vaddress.h"
#include "ibrcommon/link/LinkMonitor.h"
#include <list>
#include <set>

#include <winsock2.h>
#include <ddk/ntddndis.h>
#include <iphlpapi.h>

namespace ibrcommon
{
	class Win32LinkManager : public LinkManager
	{
	public:
		Win32LinkManager();
		virtual ~Win32LinkManager();

		void up() throw();
		void down() throw();

		const vinterface getInterface(int index) const;
		const std::list<vaddress> getAddressList(const vinterface &iface, const std::string &scope = "");

		std::set<ibrcommon::vinterface> getInterfaces() const;

	private:
		void freeAdapterInfo(IP_ADAPTER_ADDRESSES *pAddresses) const;
		IP_ADAPTER_ADDRESSES* getAdapterInfo() const;
		vaddress getAddress(SOCKET_ADDRESS &address) const;

		LinkMonitor _lm;
	};
} /* namespace ibrcommon */
#endif /* DEFAULTLINKMANAGER_H_ */
