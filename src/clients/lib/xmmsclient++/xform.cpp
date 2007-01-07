/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2007 XMMS2 Team
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
#include <xmmsclient/xmmsclient++/dict.h>
#include <xmmsclient/xmmsclient++/typedefs.h>
#include <xmmsclient/xmmsclient++/helpers.h>
#include <xmmsclient/xmmsclient++/list.h>
#include <xmmsclient/xmmsclient++/xform.h>

#include <boost/function.hpp>
#include <boost/bind.hpp>

#include <string>

namespace Xmms
{

	Xform::~Xform()
	{
	}

	List< Dict >
	Xform::browse( const std::string& url ) const
	{

		xmmsc_result_t* res =
		    call( connected_, ml_,
		          boost::bind( xmmsc_xform_media_browse, conn_, url.c_str() )
		        );

		List< Dict > result( res );
		xmmsc_result_unref( res );

		return res;
	}

	List< Dict >
	Xform::browseEncoded( const std::string& url ) const
	{

		xmmsc_result_t* res =
		    call( connected_, ml_,
		          boost::bind( xmmsc_xform_media_browse_encoded,
		                       conn_, url.c_str() )
		        );

		List< Dict > result( res );
		xmmsc_result_unref( res );

		return res;
	}

	void
	Xform::browse( const std::string& url, const DictListSlot& slot,
	               const ErrorSlot& error ) const
	{

		aCall<DictList>( connected_, 
							 boost::bind( xmmsc_xform_media_browse, conn_,
										  url.c_str() ),
							 slot, error );

	}

	void
	Xform::browseEncoded( const std::string& url, const DictListSlot& slot,
	                      const ErrorSlot& error ) const
	{

		aCall<DictList>( connected_,
		                 boost::bind( xmmsc_xform_media_browse_encoded,
		                 conn_, url.c_str() ),
		                 slot, error );

	}

	Xform::Xform( xmmsc_connection_t*& conn, bool& connected,
				  MainloopInterface*& ml ) :
		conn_( conn ), connected_( connected ), ml_( ml )
	{
	}

}
