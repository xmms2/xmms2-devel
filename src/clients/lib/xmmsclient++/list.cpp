/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2009 XMMS2 Team
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

	SuperList::SuperList( xmmsv_t* value )
		: value_( 0 )
	{

		if( xmmsv_is_error( value ) ) {
			const char *buf;
			xmmsv_get_error( value, &buf );
			throw value_error( buf );
		}
		if( !xmmsv_is_list( value ) ) {
			throw not_list_error( "Provided value is not a list" );
		}

		value_ = value;
		xmmsv_ref( value_ );

		xmmsv_get_list_iter( value_, &iter_ );

	}

	SuperList::SuperList( const SuperList& list )
		: value_( list.value_ )
	{
		xmmsv_ref( value_ );
	}

	SuperList& SuperList::operator=( const SuperList& list )
	{
		value_ = list.value_;
		xmmsv_ref( value_ );
		xmmsv_get_list_iter( value_, &iter_ );
		return *this;
	}

	SuperList::~SuperList()
	{
		xmmsv_unref( value_ );
	}

	void SuperList::first() const
	{
		xmmsv_list_iter_first( iter_ );
	}

	void SuperList::operator++() const
	{
		xmmsv_list_iter_next( iter_ );
	}

	bool SuperList::isValid() const
	{
		return xmmsv_list_iter_valid( iter_ );
	}

	xmmsv_t* SuperList::getElement() const
	{
		xmmsv_t *elem = NULL;
		xmmsv_list_iter_entry( iter_, &elem );
		return elem;
	}

}
