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
#include <xmmsclient/xmmsclient++/client.h>
#include <xmmsclient/xmmsclient++/collection.h>
#include <xmmsclient/xmmsclient++/helpers.h>
#include <xmmsclient/xmmsclient++/exceptions.h>
#include <xmmsclient/xmmsclient++/result.h>

#include <string>

#include <boost/bind.hpp>

namespace Xmms
{

	const Collection::Namespace Collection::ALL         = XMMS_COLLECTION_NS_ALL;
	const Collection::Namespace Collection::COLLECTIONS = XMMS_COLLECTION_NS_COLLECTIONS;
	const Collection::Namespace Collection::PLAYLISTS   = XMMS_COLLECTION_NS_PLAYLISTS;

	Collection::~Collection()
	{
	}

	CollResult
	Collection::get( const std::string& name, Namespace nsname ) const
	{
		xmmsc_result_t* res
		    = call( connected_,
		            boost::bind( xmmsc_coll_get, conn_, name.c_str(), nsname ) );
		return CollResult( res, ml_ );
	}

	StringListResult
	Collection::list( Namespace nsname ) const
	{
		xmmsc_result_t* res
		    = call( connected_,
		            boost::bind( xmmsc_coll_list, conn_, nsname ) );
		return StringListResult( res, ml_ );
	}

	VoidResult
	Collection::save( const Coll::Coll& coll, const std::string& name,
	                  Namespace nsname ) const
	{
		xmmsc_result_t* res =
		    call( connected_,
		          boost::bind( xmmsc_coll_save, conn_,
		                       coll.coll_, name.c_str(), nsname ) );
		return VoidResult( res, ml_ );
	}

	VoidResult
	Collection::remove( const std::string& name, Namespace nsname ) const
	{
		xmmsc_result_t* res =
		    call( connected_,
		          boost::bind( xmmsc_coll_remove, conn_, name.c_str(), nsname ) );
		return VoidResult( res, ml_ );
	}

	StringListResult
	Collection::find( unsigned int id, Namespace nsname ) const
	{
		xmmsc_result_t* res
		    = call( connected_,
		            boost::bind( xmmsc_coll_find, conn_, id, nsname ) );
		return StringListResult( res, ml_ );
	}

	VoidResult
	Collection::rename( const std::string& from_name,
	                    const std::string& to_name,
	                    Namespace nsname ) const
	{
		xmmsc_result_t* res =
		    call( connected_,
		          boost::bind( xmmsc_coll_rename, conn_, from_name.c_str(),
		                       to_name.c_str(), nsname ) );
		return VoidResult( res, ml_ );
	}

	CollResult
	Collection::idlistFromPlaylistFile( const std::string& path ) const {
		xmmsc_result_t* res
			= call( connected_,
			        boost::bind( xmmsc_coll_idlist_from_playlist_file, conn_,
			                     path.c_str() ) );
		return CollResult( res, ml_ );
	}

	UintListResult
	Collection::queryIds( const Coll::Coll& coll,
	                      const std::list< std::string >& order,
	                      unsigned int limit_len,
	                      unsigned int limit_start ) const
	{
		xmmsv_t *xorder = makeStringList( order );

		xmmsc_result_t* res
		    = call( connected_,
		            boost::bind( xmmsc_coll_query_ids, conn_, coll.coll_,
		                         xorder, limit_start, limit_len ) );

		xmmsv_unref( xorder );

		return UintListResult( res, ml_ );
	}

	DictListResult
	Collection::queryInfos( const Coll::Coll& coll,
	                        const std::list< std::string >& fetch,
	                        const std::list< std::string >& order,
	                        unsigned int limit_len,
	                        unsigned int limit_start,
	                        const std::list< std::string >& group
	                      ) const
	{
		assertNonEmptyFetchList( fetch );

		xmmsv_t *xorder, *xfetch, *xgroup;
		xorder = makeStringList( order );
		xfetch = makeStringList( fetch );
		xgroup = makeStringList( group );

		xmmsc_result_t* res
		    = call( connected_,
		            boost::bind( xmmsc_coll_query_infos, conn_, coll.coll_,
		                         xorder, limit_start, limit_len,
		                         xfetch, xgroup ) );

		xmmsv_unref( xorder );
		xmmsv_unref( xfetch );
		xmmsv_unref( xgroup );

		return DictListResult( res, ml_ );
	}

	CollPtr
	Collection::parse( const std::string& pattern ) const
	{
		xmmsv_coll_t* coll;

		if( !xmmsv_coll_parse( pattern.c_str(), &coll ) ) {
			throw collection_parsing_error( "invalid collection pattern" );
		}

		return CollResult::createColl( coll );
	}

	DictSignal
	Collection::broadcastCollectionChanged() const
	{
		xmmsc_result_t* res =
		    call( connected_,
		          boost::bind( xmmsc_broadcast_collection_changed, conn_ ) );
		return DictSignal( res, ml_ );
	}

	void
	Collection::assertNonEmptyFetchList( const std::list< std::string >& l
	                                   ) const
	{
		if( l.size() == 0 ) {
			throw argument_error( "fetch list cannot be empty!" );
		}
	}

	Collection::Collection( xmmsc_connection_t*& conn, bool& connected,
	                        MainloopInterface*& ml )
	    : conn_( conn ), connected_( connected ), ml_( ml )
	{
	}

}
