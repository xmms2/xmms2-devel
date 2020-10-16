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
#include <xmmsclient/xmmsclient++/listener.h>

namespace Xmms
{

	bool
	ListenerInterface::operator==( const ListenerInterface& rhs ) const
	{
		return ( getFileDescriptor() == rhs.getFileDescriptor() );
	}



	Listener::Listener( const Listener& src )
		: ListenerInterface(), conn_( src.conn_ )
	{
		xmmsc_ref( conn_ );
	}

	Listener& Listener::operator=( const Listener& src )
	{
		if (conn_ != NULL)
			xmmsc_unref( conn_ );
		conn_ = src.conn_;
		xmmsc_ref( conn_ );
		return *this;
	}

	Listener::~Listener()
	{
		xmmsc_unref (conn_);
	}

	int32_t
	Listener::getFileDescriptor() const
	{
		return xmmsc_io_fd_get( conn_ );
	}

	bool
	Listener::listenIn() const
	{
		return true;
	}

	bool
	Listener::listenOut() const
	{
		return xmmsc_io_want_out( conn_ );
	}

	void
	Listener::handleIn()
	{
		xmmsc_io_in_handle( conn_ );
	}

	void
	Listener::handleOut()
	{
		xmmsc_io_out_handle( conn_ );
	}

	Listener::Listener( xmmsc_connection_t*& conn ) :
		conn_( conn )
	{
		xmmsc_ref (conn);
	}

}
