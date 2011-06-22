/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2011 XMMS2 Team
 *
 *  PLUGINS ARE NOT CONSIDERED TO BE DERIVED WORK !!!
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 */

#include <xmmsclient/xmmsclient++/signal.h>
#include <xmmsclient/xmmsclient++/coll.h>

namespace Xmms
{

	SignalHolder& SignalHolder::getInstance()
	{
		static SignalHolder instance;
		return instance;
	}

	void SignalHolder::addSignal( SignalInterface* sig )
	{
		signals_.push_back( sig );
	}

	void SignalHolder::removeSignal( SignalInterface* sig )
	{
		signals_.remove( sig );
		delete sig;
	}

	SignalHolder::~SignalHolder()
	{
		deleteAll();
	}

	void SignalHolder::deleteAll()
	{
		std::list< SignalInterface* >::iterator i;
		for( i = signals_.begin(); i != signals_.end(); ++i )
		{
			delete *i; *i = 0;
		}
		signals_.clear();
	}

	Coll::Coll* extract_collection( xmmsv_t* val )
	{
		Coll::Coll* temp = 0;
		xmmsv_coll_t* coll = 0;
		xmmsv_get_coll( val, &coll );
		switch( xmmsv_coll_get_type( coll ) ) {

			case XMMS_COLLECTION_TYPE_REFERENCE: {
				temp = new Coll::Reference( coll );
				break;
			}
			case XMMS_COLLECTION_TYPE_UNION: {
				temp = new Coll::Union( coll );
				break;
			}
			case XMMS_COLLECTION_TYPE_INTERSECTION: {
				temp = new Coll::Intersection( coll );
				break;
			}
			case XMMS_COLLECTION_TYPE_COMPLEMENT: {
				temp = new Coll::Complement( coll );
				break;
			}
			case XMMS_COLLECTION_TYPE_HAS: {
				temp = new Coll::Has( coll );
				break;
			}
			case XMMS_COLLECTION_TYPE_SMALLER: {
				temp = new Coll::Smaller( coll );
				break;
			}
			case XMMS_COLLECTION_TYPE_GREATER: {
				temp = new Coll::Greater( coll );
				break;
			}
			case XMMS_COLLECTION_TYPE_EQUALS: {
				temp = new Coll::Equals( coll );
				break;
			}
			case XMMS_COLLECTION_TYPE_MATCH: {
				temp = new Coll::Match( coll );
				break;
			}
			case XMMS_COLLECTION_TYPE_IDLIST: {
				temp = new Coll::Idlist( coll );
				break;
			}
			case XMMS_COLLECTION_TYPE_QUEUE: {
				temp = new Coll::Queue( coll );
				break;
			}
			case XMMS_COLLECTION_TYPE_PARTYSHUFFLE: {
				temp = new Coll::PartyShuffle( coll );
				break;
			}

		}

		return temp;
	}

	void disconnect_callback( void* userdata )
	{

		DisconnectCallback* temp = static_cast< DisconnectCallback* >( userdata );
		for( DisconnectCallback::const_iterator i = temp->begin();
			 i != temp->end(); ++i )
		{
			(*i)();
		}

	}

}
