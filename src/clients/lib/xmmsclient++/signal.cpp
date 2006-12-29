/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2006 XMMS2 Team
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

	void disconnect_callback( void* userdata )
	{

		(*(static_cast< DisconnectCallback* >( userdata )))();

	}

}
