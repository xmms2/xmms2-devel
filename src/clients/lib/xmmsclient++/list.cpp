/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2008 XMMS2 Team
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

	SuperList::SuperList( xmmsc_result_t* result )
		: result_( 0 )
	{

		if( xmmsc_result_iserror( result ) ) {
			throw result_error( xmmsc_result_get_error( result ) );
		}
		if( !xmmsc_result_is_list( result ) ) {
			throw not_list_error( "Provided result is not a list" );
		}

		result_ = result;
		xmmsc_result_ref( result_ );

	}

	SuperList::SuperList( const SuperList& list )
		: result_( list.result_ )
	{
		xmmsc_result_ref( result_ );
	}

	SuperList& SuperList::operator=( const SuperList& list )
	{
		result_ = list.result_;
		xmmsc_result_ref( result_ );
		return *this;
	}

	SuperList::~SuperList()
	{
		xmmsc_result_unref( result_ );
	}

	void SuperList::first() const
	{

		if( !xmmsc_result_list_first( result_ ) ) {
			// throw something...
		}

	}

	void SuperList::operator++() const
	{
		if( !xmmsc_result_list_next( result_ ) ) {
			// throw
		}
	}

	bool SuperList::isValid() const
	{
		return xmmsc_result_list_valid( result_ );
	}

}
