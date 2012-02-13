/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2012 XMMS2 Team
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
#include <xmmsclient/xmmsclient++/bindata.h>
#include <xmmsclient/xmmsclient++/mainloop.h>
#include <xmmsclient/xmmsclient++/result.h>

#include <boost/bind.hpp>

#include <string>

namespace Xmms
{

	Bindata::~Bindata()
	{
	}

	StringResult Bindata::add( const Xmms::bin& data ) const
	{
		xmmsc_result_t* res =
			call( connected_,
			      boost::bind( xmmsc_bindata_add, conn_,
			                   data.data(), data.size() ) );
		return StringResult( res, ml_ );
	}

	BinResult Bindata::retrieve( const std::string& hash ) const
	{
		xmmsc_result_t* res =
			call( connected_,
			      boost::bind( xmmsc_bindata_retrieve, conn_, hash.c_str() ) );
		return BinResult( res, ml_ );
	}

	VoidResult Bindata::remove( const std::string& hash ) const
	{
		xmmsc_result_t* res =
		    call( connected_,
		          boost::bind( xmmsc_bindata_remove, conn_, hash.c_str() ) );
		return VoidResult( res, ml_ );
	}

	StringListResult Bindata::list() const
	{
		xmmsc_result_t* res =
		    call( connected_,
		          boost::bind( xmmsc_bindata_list, conn_ ) );
		return StringListResult( res, ml_ );
	}

	Bindata::Bindata( xmmsc_connection_t*& conn, bool& connected,
	                  MainloopInterface*& ml ) :
		conn_( conn ), connected_( connected ), ml_( ml )
	{
	}

}
