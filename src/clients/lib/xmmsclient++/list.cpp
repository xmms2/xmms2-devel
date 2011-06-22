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

#include <xmmsclient/xmmsclient.h>

#include <xmmsclient/xmmsclient++/list.h>
#include <xmmsclient/xmmsclient++/exceptions.h>

#include <boost/any.hpp>

#include <string>

namespace Xmms
{

	int (*type_traits< int32_t >::get_func)( const xmmsv_t*, int32_t* ) = xmmsv_get_int;
	int (*type_traits< std::string >::get_func)( const xmmsv_t*, const char** ) = xmmsv_get_string;

}
