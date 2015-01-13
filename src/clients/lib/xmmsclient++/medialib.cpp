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
#include <xmmsclient/xmmsclient++/medialib.h>
#include <xmmsclient/xmmsclient++/mainloop.h>
#include <xmmsclient/xmmsclient++/helpers.h>
#include <xmmsclient/xmmsclient++/result.h>

#include <boost/function.hpp>
#include <boost/bind.hpp>

#include <string>
#include <list>
#include <map>
#include <vector>

namespace Xmms
{

	Medialib::~Medialib()
	{
	}

	VoidResult Medialib::addEntry( const std::string& url ) const
	{
		xmmsc_result_t* res =
		    call( connected_,
		          boost::bind( xmmsc_medialib_add_entry, conn_, url.c_str() ) );
		return VoidResult( res, ml_ );
	}

	VoidResult Medialib::addEntry( const std::string& url,
	                               const std::list< std::string >& args ) const
	{
		xmmsv_t* dict = makeStringDict( args );
		xmmsc_result_t* res =
			call( connected_,
			      boost::bind( xmmsc_medialib_add_entry_full, conn_,
			                   url.c_str(), dict ) );
		xmmsv_unref( dict );
		return VoidResult( res, ml_ );
	}

	VoidResult Medialib::addEntry( const std::string& url,
	                               const std::map< std::string, Xmms::Dict::Variant >& args ) const
	{
		xmmsv_t* dict = makeStringDict( args );
		xmmsc_result_t* res =
		    call( connected_,
		          boost::bind( xmmsc_medialib_add_entry_full, conn_,
		                       url.c_str(), dict ) );
		xmmsv_unref( dict );
		return VoidResult( res, ml_ );
	}

	VoidResult Medialib::addEntryEncoded( const std::string& url ) const
	{
		xmmsc_result_t* res =
		    call( connected_,
		          boost::bind( xmmsc_medialib_add_entry_encoded,
		                       conn_, url.c_str() ) );
		return VoidResult( res, ml_ );
	}

	VoidResult Medialib::entryPropertyRemove( int id,
	                                          const std::string& key,
	                                          const std::string& source ) const
	{
		boost::function< xmmsc_result_t*() > f;

		using boost::bind;
		if( source.empty() ) {
			f = bind( xmmsc_medialib_entry_property_remove,
			          conn_, id, key.c_str() );
		}
		else {
			f = bind( xmmsc_medialib_entry_property_remove_with_source,
			          conn_, id, source.c_str(), key.c_str() );
		}

		xmmsc_result_t* res = call( connected_, f );
		return VoidResult( res, ml_ );
	}

	VoidResult Medialib::entryPropertySet( int id,
	                                       const std::string& key,
	                                       const std::string& value,
	                                       const std::string& source ) const
	{
		boost::function< xmmsc_result_t*() > f;

		using boost::bind;
		if( source.empty() ) {
			f = bind( xmmsc_medialib_entry_property_set_str,
			          conn_, id, key.c_str(), value.c_str() );
		}
		else {
			f = bind( xmmsc_medialib_entry_property_set_str_with_source,
			          conn_, id, source.c_str(), key.c_str(), value.c_str() );
		}

		xmmsc_result_t* res = call( connected_, f );
		return VoidResult( res, ml_ );
	}

	VoidResult Medialib::entryPropertySet( int id,
	                                       const std::string& key,
	                                       int32_t value,
	                                       const std::string& source ) const
	{
		boost::function< xmmsc_result_t*() > f;

		using boost::bind;
		if( source.empty() ) {
			f = bind( xmmsc_medialib_entry_property_set_int,
			          conn_, id, key.c_str(), value );
		}
		else {
			f = bind( xmmsc_medialib_entry_property_set_int_with_source,
			          conn_, id, source.c_str(), key.c_str(), value );
		}

		xmmsc_result_t* res = call( connected_, f );
		return VoidResult( res, ml_ );
	}


	IntResult Medialib::getID( const std::string& url ) const
	{
		xmmsc_result_t* res = call( connected_,
		                            boost::bind( xmmsc_medialib_get_id,
		                                         conn_, url.c_str() )
		                          );
		return IntResult( res, ml_ );
	}

	PropDictResult Medialib::getInfo( int id ) const
	{
		xmmsc_result_t* res = call( connected_,
		                            boost::bind( xmmsc_medialib_get_info,
		                                         conn_, id )
		                          );
		return PropDictResult( res, ml_ );
	}

	VoidResult Medialib::pathImport( const std::string& path ) const
	{
		xmmsc_result_t* res =
		    call( connected_,
		          boost::bind( xmmsc_medialib_import_path, conn_, path.c_str() )
		        );
		return VoidResult( res, ml_ );
	}

	VoidResult Medialib::pathImportEncoded( const std::string& path ) const
	{
		xmmsc_result_t* res =
		    call( connected_,
		          boost::bind( xmmsc_medialib_import_path_encoded,
		                       conn_, path.c_str() ) );
		return VoidResult( res, ml_ );
	}

	VoidResult Medialib::rehash( int id ) const
	{
		xmmsc_result_t* res =
		    call( connected_,
		          boost::bind( xmmsc_medialib_rehash, conn_, id ) );
		return VoidResult( res, ml_ );
	}

	VoidResult Medialib::removeEntry( int id ) const
	{
		xmmsc_result_t* res =
		    call( connected_,
		          boost::bind( xmmsc_medialib_remove_entry, conn_, id ) );
		return VoidResult( res, ml_ );
	}

	VoidResult Medialib::moveEntry( int id,
	                                const std::string& path ) const
	{
		xmmsc_result_t* res =
		    call( connected_,
		          boost::bind( xmmsc_medialib_move_entry, conn_,
		                       id, path.c_str() ) );
		return VoidResult( res, ml_ );
	}

	IntSignal Medialib::broadcastEntryAdded() const
	{
		xmmsc_result_t* res =
		    call( connected_,
		          boost::bind( xmmsc_broadcast_medialib_entry_added, conn_ ) );
		return IntSignal( res, ml_ );
	}

	IntSignal Medialib::broadcastEntryChanged() const
	{
		using boost::bind;
		xmmsc_result_t* res =
		    call( connected_,
		          bind( xmmsc_broadcast_medialib_entry_updated, conn_ ) );
		return IntSignal( res, ml_ );
	}

	IntSignal Medialib::broadcastEntryUpdated() const
	{
		using boost::bind;
		xmmsc_result_t* res =
			call( connected_,
			      bind( xmmsc_broadcast_medialib_entry_updated, conn_ ) );
		return IntSignal( res, ml_ );
	}

	IntSignal Medialib::broadcastEntryRemoved() const
	{
		xmmsc_result_t* res =
			call( connected_,
			      boost::bind( xmmsc_broadcast_medialib_entry_removed, conn_ ) );
		return IntSignal( res, ml_ );
	}

	Medialib::Medialib( xmmsc_connection_t*& conn, bool& connected,
	                    MainloopInterface*& ml ) :
		conn_( conn ), connected_( connected ), ml_( ml )
	{
	}

}
