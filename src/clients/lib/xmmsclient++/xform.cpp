/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2020 XMMS2 Team
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
#include <xmmsclient/xmmsclient++/medialib.h>
#include <xmmsclient/xmmsclient++/mainloop.h>
#include <xmmsclient/xmmsclient++/helpers.h>
#include <xmmsclient/xmmsclient++/result.h>
#include <xmmsclient/xmmsclient++/xform.h>

#include <boost/function.hpp>
#include <boost/bind.hpp>

#include <string>

namespace Xmms
{

	Xform::~Xform()
	{
	}

	DictListResult
	Xform::browse( const std::string& url ) const
	{
		xmmsc_result_t* res =
		    call( connected_, 
		          boost::bind( xmmsc_xform_media_browse, conn_, url.c_str() ) );
		return DictListResult( res, ml_ );
	}

	DictListResult
	Xform::browseEncoded( const std::string& url ) const
	{
		xmmsc_result_t* res =
		    call( connected_,
		          boost::bind( xmmsc_xform_media_browse_encoded,
		                       conn_, url.c_str() ) );
		return DictListResult( res, ml_ );
	}

	Xform::Xform( xmmsc_connection_t*& conn, bool& connected,
				  MainloopInterface*& ml ) :
		conn_( conn ), connected_( connected ), ml_( ml )
	{
	}

}
