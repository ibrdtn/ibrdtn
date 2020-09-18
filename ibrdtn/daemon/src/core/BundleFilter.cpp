/*
 * BundleFilter.cpp
 *
 * Copyright (C) 2014 IBR, TU Braunschweig
 *
 * Written-by: Johannes Morgenroth <jm@m-network.de>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include "core/BundleFilter.h"

namespace dtn
{
	namespace core
	{
		FilterContext::FilterContext()
		: _metabundle(NULL), _bundle(NULL), _primaryblock(NULL), _block(NULL),
		_block_length(0), _peer(NULL), _protocol(dtn::core::Node::CONN_UNDEFINED), _routing(NULL)
		{

		}

		FilterContext::~FilterContext()
		{

		}

		void FilterContext::setMetaBundle(const dtn::data::MetaBundle &data)
		{
			_metabundle = &data;
		}

		const dtn::data::MetaBundle& FilterContext::getMetaBundle() const noexcept (false)
		{
			if (_metabundle == NULL) throw FilterException("attribute not present in this context");
			return *_metabundle;
		}

		void FilterContext::setBundle(const dtn::data::Bundle &data)
		{
			_bundle = &data;
			_primaryblock = &data;
		}

		const dtn::data::Bundle& FilterContext::getBundle() const noexcept (false)
		{
			if (_bundle == NULL) throw FilterException("attribute not present in this context");
			return *_bundle;
		}

		const dtn::data::BundleID& FilterContext::getBundleID() const noexcept (false)
		{
			if (_metabundle != NULL) return *_metabundle;
			if (_primaryblock != NULL) return *_primaryblock;
			throw FilterException("attribute not present in this context");
		}

		void FilterContext::setPrimaryBlock(const dtn::data::PrimaryBlock &data)
		{
			_primaryblock = &data;
		}

		const dtn::data::PrimaryBlock& FilterContext::getPrimaryBlock() const noexcept (false)
		{
			if (_primaryblock == NULL) throw FilterException("attribute not present in this context");
			return *_primaryblock;
		}

		void FilterContext::setBlock(const dtn::data::Block &block, const dtn::data::Number &size)
		{
			_block = &block;
			_block_length = size;
		}

		const dtn::data::Block& FilterContext::getBlock() const noexcept (false)
		{
			if (_block == NULL) throw FilterException("attribute not present in this context");
			return *_block;
		}

		dtn::data::Number FilterContext::getBlockLength() const noexcept (false)
		{
			if (_block == NULL) throw FilterException("attribute not present in this context");
			return _block_length;
		}

		void FilterContext::setPeer(const dtn::data::EID &endpoint)
		{
			_peer = &endpoint;
		}

		const dtn::data::EID& FilterContext::setPeer() const noexcept (false)
		{
			if (_peer == NULL) throw FilterException("attribute not present in this context");
			return *_peer;
		}

		void FilterContext::setProtocol(const dtn::core::Node::Protocol &protocol)
		{
			_protocol = protocol;
		}

		dtn::core::Node::Protocol FilterContext::getProtocol() const noexcept (false)
		{
			if (_protocol == dtn::core::Node::CONN_UNDEFINED) throw FilterException("attribute not present in this context");
			return _protocol;
		}

		void FilterContext::setRouting(const dtn::routing::RoutingExtension &routing)
		{
			_routing = &routing;
		}

		const std::string FilterContext::getRoutingTag() const noexcept (false)
		{
			if (_routing == NULL) throw FilterException("attribute not present in this context");
			return _routing->getTag();
		}

		BundleFilter::BundleFilter()
		 : _next(NULL)
		{
		}

		BundleFilter::~BundleFilter()
		{
			if (_next != NULL) delete _next;
		}

		BundleFilter* BundleFilter::append(BundleFilter *filter)
		{
			if (_next != NULL) {
				_next->append(filter);
				return this;
			}
			_next = filter;
			return this;
		}

		BundleFilter::ACTION BundleFilter::evaluate(const FilterContext &context) const noexcept
		{
			return (_next == NULL) ? BundleFilter::PASS : _next->evaluate(context);
		}

		BundleFilter::ACTION BundleFilter::filter(const FilterContext &context, dtn::data::Bundle &bundle) const noexcept
		{
			return (_next == NULL) ? BundleFilter::PASS : _next->filter(context, bundle);
		}
	} /* namespace core */
} /* namespace dtn */
