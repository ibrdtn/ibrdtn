/*
 * AdministrativeBlock.h
 *
 *  Created on: 10.01.2013
 *      Author: morgenro
 */

#ifndef ADMINISTRATIVEBLOCK_H_
#define ADMINISTRATIVEBLOCK_H_

#include <ibrdtn/data/PayloadBlock.h>

namespace dtn
{
	namespace data
	{
		class AdministrativeBlock {
		public:
			class WrongRecordException : public ibrcommon::Exception
			{
			public:
				WrongRecordException(std::string what = "This administrative block is not of the expected type.") throw() : ibrcommon::Exception(what)
				{
				};
			};

			AdministrativeBlock();
			virtual ~AdministrativeBlock() = 0;

			virtual void read(const dtn::data::PayloadBlock &p) throw (WrongRecordException) = 0;
			virtual void write(dtn::data::PayloadBlock &p) const = 0;
		};
	} /* namespace data */
} /* namespace dtn */
#endif /* ADMINISTRATIVEBLOCK_H_ */
