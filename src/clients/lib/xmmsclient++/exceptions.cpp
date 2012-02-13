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

#include <xmmsclient/xmmsclient++/exceptions.h>

#include <stdexcept>
#include <string>
using std::string;

namespace Xmms
{

	connection_error::connection_error( const string& what_arg ) :
		runtime_error( what_arg )
	{
	}

	not_list_error::not_list_error( const string& what_arg ) :
		logic_error( what_arg )
	{
	}

	result_error::result_error( const string& what_arg ) :
		runtime_error( what_arg )
	{
	}

	value_error::value_error( const string& what_arg ) :
		runtime_error( what_arg )
	{
	}

	argument_error::argument_error( const string& what_arg ) :
		runtime_error( what_arg )
	{
	}

	not_dict_error::not_dict_error( const string& what_arg ) :
		logic_error( what_arg )
	{
	}

	no_such_key_error::no_such_key_error( const string& what_arg ) :
		runtime_error( what_arg )
	{
	}

	mainloop_running_error::mainloop_running_error( const string& what_arg ) :
		logic_error( what_arg )
	{
	}

	wrong_type_error::wrong_type_error( const std::string& what_arg ) :
		logic_error( what_arg )
	{
	}

	missing_operand_error::missing_operand_error( const string& what_arg ) :
		runtime_error( what_arg )
	{
	}

	out_of_range::out_of_range( const std::string& what_arg ) :
		std::out_of_range( what_arg )
	{
	}

	collection_type_error::collection_type_error( const string& what_arg ) :
		runtime_error( what_arg )
	{
	}

	collection_operation_error::collection_operation_error( const string& what_arg ) :
		runtime_error( what_arg )
	{
	}

	collection_parsing_error::collection_parsing_error( const string& what_arg ) :
		runtime_error( what_arg )
	{
	}

	invalid_url::invalid_url( const string& what_arg ) :
		runtime_error( what_arg )
	{
	}

}
