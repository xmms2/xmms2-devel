/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2013 XMMS2 Team
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
#include <xmmsclient/xmmsclient++/stats.h>
#include <xmmsclient/xmmsclient++/signal.h>
#include <xmmsclient/xmmsclient++/helpers.h>
#include <xmmsclient/xmmsclient++/mainloop.h>
#include <xmmsclient/xmmsclient++/typedefs.h>

#include <boost/bind.hpp>

#include <list>

namespace Xmms
{
	
	Stats::~Stats()
	{
	}

	DictResult
	Stats::mainStats() const
	{
		xmmsc_result_t* res = 
		    call( connected_, boost::bind( xmmsc_main_stats, conn_ ) );
		return DictResult( res, ml_ );
	}

	DictListResult Stats::pluginList(Plugins::Type type) const
	{
		xmmsc_result_t* res = 
		    call( connected_, boost::bind( xmmsc_main_list_plugins, conn_, type ) );
		return DictListResult( res, ml_ );
	}

	ReaderStatusSignal
	Stats::broadcastMediainfoReaderStatus() const
	{
		using boost::bind;
		xmmsc_result_t* res =
		    call( connected_,
		          bind( xmmsc_broadcast_mediainfo_reader_status, conn_) );
		return ReaderStatusSignal( res, ml_ );
	}

	IntSignal
	Stats::signalMediainfoReaderUnindexed() const
	{
		using boost::bind;
		xmmsc_result_t* res =
		    call( connected_,
		          bind( xmmsc_signal_mediainfo_reader_unindexed, conn_) );
		return IntSignal( res, ml_ );
	}

	Stats::Stats( xmmsc_connection_t*& conn, bool& connected,
	              MainloopInterface*& ml ) :
		conn_( conn ), connected_( connected ), ml_( ml )
	{
	}

}
