/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2015 XMMS2 Team
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

#include <xmmsclient/xmmsclient.h>
#include <xmmsclient/xmmsclient++/client.h>
#include <xmmsclient/xmmsclient++/config.h>
#include <xmmsclient/xmmsclient++/helpers.h>
#include <xmmsclient/xmmsclient++/mainloop.h>

#include <boost/bind.hpp>

#include <list>
#include <string>

namespace Xmms
{
	
	Config::~Config()
	{
	}

	VoidResult
	Config::valueRegister( const std::string& name,
	                       const std::string& defval ) const
	{
		xmmsc_result_t* res =
		    call( connected_,
		          boost::bind( xmmsc_config_register_value, conn_,
		                       name.c_str(), defval.c_str() ) );
		return VoidResult( res, ml_ );
	}

	VoidResult
	Config::valueSet( const std::string& key,
	                  const std::string& value ) const
	{
		xmmsc_result_t* res =
		    call( connected_,
		          boost::bind( xmmsc_config_set_value, conn_,
		                       key.c_str(), value.c_str() ) );
		return VoidResult( res, ml_ );
	}

	StringResult
	Config::valueGet( const std::string& key ) const
	{
		xmmsc_result_t* res = 
		    call( connected_,
		          boost::bind( xmmsc_config_get_value, conn_, key.c_str() ) );
		return StringResult( res, ml_ );
	}

	DictResult
	Config::valueList() const
	{
		xmmsc_result_t* res = call( connected_,
		                            boost::bind( xmmsc_config_list_values, conn_ ));
		return DictResult( res, ml_ );
	}

	DictSignal
	Config::broadcastValueChanged() const
	{
		xmmsc_result_t* res =
		    call( connected_,
		          boost::bind( xmmsc_broadcast_config_value_changed, conn_ ) );
		return DictSignal( res, ml_ );
	}


	Config::Config( xmmsc_connection_t*& conn, bool& connected,
	                MainloopInterface*& ml ) :
		conn_( conn ), connected_( connected ), ml_( ml )
	{
	}

}
