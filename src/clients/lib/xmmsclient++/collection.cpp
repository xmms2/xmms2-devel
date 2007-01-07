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
#include <xmmsclient/xmmsclient++/client.h>
#include <xmmsclient/xmmsclient++/collection.h>
#include <xmmsclient/xmmsclient++/coll.h>
#include <xmmsclient/xmmsclient++/helpers.h>
#include <xmmsclient/xmmsclient++/list.h>
#include <xmmsclient/xmmsclient++/dict.h>

#include <string>

#include <boost/bind.hpp>

namespace Xmms
{

	const Collection::Namespace Collection::ALL         = XMMS_COLLECTION_NS_ALL;
	const Collection::Namespace Collection::COLLECTIONS = XMMS_COLLECTION_NS_COLLECTIONS;
	const Collection::Namespace Collection::PLAYLISTS   = XMMS_COLLECTION_NS_PLAYLISTS;

	CollPtr
	Collection::createColl( xmmsc_result_t* res ) {

		xmmsc_coll_t* coll = 0;
		if( !xmmsc_result_get_collection( res, &coll ) ) {
			throw result_error( "invalid collection in result" );
		}

		CollPtr collptr( createColl( coll ) );

		xmmsc_result_unref( res );

		return collptr;
	}

	CollPtr
	Collection::createColl( xmmsc_coll_t* coll ) {

		CollPtr collptr;

		switch( xmmsc_coll_get_type( coll ) ) {

			case XMMS_COLLECTION_TYPE_REFERENCE: {
				collptr.reset( new Coll::Reference( coll ) );
				break;
			}
			case XMMS_COLLECTION_TYPE_UNION: {
				collptr.reset( new Coll::Union( coll ) );
				break;
			}
			case XMMS_COLLECTION_TYPE_INTERSECTION: {
				collptr.reset( new Coll::Intersection( coll ) );
				break;
			}
			case XMMS_COLLECTION_TYPE_COMPLEMENT: {
				collptr.reset( new Coll::Complement( coll ) );
				break;
			}
			case XMMS_COLLECTION_TYPE_HAS: {
				collptr.reset( new Coll::Has( coll ) );
				break;
			}
			case XMMS_COLLECTION_TYPE_SMALLER: {
				collptr.reset( new Coll::Smaller( coll ) );
				break;
			}
			case XMMS_COLLECTION_TYPE_GREATER: {
				collptr.reset( new Coll::Greater( coll ) );
				break;
			}
			case XMMS_COLLECTION_TYPE_MATCH: {
				collptr.reset( new Coll::Match( coll ) );
				break;
			}
			case XMMS_COLLECTION_TYPE_CONTAINS: {
				collptr.reset( new Coll::Contains( coll ) );
				break;
			}
			case XMMS_COLLECTION_TYPE_IDLIST: {
				collptr.reset( new Coll::Idlist( coll ) );
				break;
			}
			case XMMS_COLLECTION_TYPE_QUEUE: {
				collptr.reset( new Coll::Queue( coll ) );
				break;
			}
			case XMMS_COLLECTION_TYPE_PARTYSHUFFLE: {
				collptr.reset( new Coll::PartyShuffle( coll ) );
				break;
			}
			case XMMS_COLLECTION_TYPE_ERROR: {
				throw result_error( "invalid collection in result" );
				break;
			}

		}

		return collptr;

	}


	Collection::~Collection() {

	}

	CollPtr
	Collection::get( const std::string& name, Namespace nsname ) const {

		xmmsc_result_t* res
		    = call( connected_, ml_,
		            boost::bind( xmmsc_coll_get, conn_, name.c_str(), nsname ) );
		return createColl( res );

	}

	List< std::string >
	Collection::list( Namespace nsname ) const {

		xmmsc_result_t* res
		    = call( connected_, ml_,
		            boost::bind( xmmsc_coll_list, conn_, nsname ) );

		List< std::string > result( res );
		xmmsc_result_unref( res );

		return result;
	}

	void
	Collection::save( const Coll::Coll& coll, const std::string& name,
	                  Namespace nsname ) const {

		vCall( connected_, ml_,
		       boost::bind( xmmsc_coll_save, conn_, coll.coll_, name.c_str(), nsname ) );

	}

	void
	Collection::remove( const std::string& name, Namespace nsname ) const {

		vCall( connected_, ml_,
		       boost::bind( xmmsc_coll_remove, conn_, name.c_str(), nsname ) );

	}

	List< std::string >
	Collection::find( unsigned int id, Namespace nsname ) const {

		xmmsc_result_t* res
		    = call( connected_, ml_,
		            boost::bind( xmmsc_coll_find, conn_, id, nsname ) );

		List< std::string > result( res );
		xmmsc_result_unref( res );

		return result;
	}

	void
	Collection::rename( const std::string& from_name,
	                    const std::string& to_name,
	                    Namespace nsname ) const {

		vCall( connected_, ml_,
		       boost::bind( xmmsc_coll_rename, conn_, from_name.c_str(),
		                    to_name.c_str(), nsname ) );

	}

	CollPtr
	Collection::idlistFromPlaylistFile( const std::string& path ) const {

		xmmsc_result_t* res
			= call( connected_, ml_,
			        boost::bind( xmmsc_coll_idlist_from_playlist_file, conn_,
			                     path.c_str() ) );

		return createColl( res );

	}

	List< unsigned int >
	Collection::queryIds( const Coll::Coll& coll,
	                      const std::list< std::string >& order,
	                      unsigned int limit_len,
	                      unsigned int limit_start ) const {

		std::vector< const char* > corder;
		fillCharArray( order, corder );

		xmmsc_result_t* res
		    = call( connected_, ml_,
		            boost::bind( xmmsc_coll_query_ids, conn_, coll.coll_,
		                         &corder[0], limit_start, limit_len ) );

		List< unsigned int > result( res );
		xmmsc_result_unref( res );

		return result;

	}

	List< Dict >
	Collection::queryInfos( const Coll::Coll& coll,
	                        const std::list< std::string >& order,
	                        unsigned int limit_len,
	                        unsigned int limit_start,
	                        const std::list< std::string >& fetch,
	                        const std::list< std::string >& group
	                      ) const {

		std::vector< const char* > corder, cfetch, cgroup;
		fillCharArray( order, corder );
		fillCharArray( fetch, cfetch );
		fillCharArray( group, cgroup );

		xmmsc_result_t* res
		    = call( connected_, ml_,
		            boost::bind( xmmsc_coll_query_infos, conn_, coll.coll_,
		                         &corder[0], limit_start, limit_len,
		                         &cfetch[0], &cgroup[0] ) );

		List< Dict > result( res );
		xmmsc_result_unref( res );

		return result;

	}

	CollPtr
	Collection::parse( const std::string& pattern ) const {

		xmmsc_coll_t* coll;

		if( !xmmsc_coll_parse( pattern.c_str(), &coll ) ) {
			throw collection_parsing_error( "invalid collection pattern" );
		}

		return createColl( coll );

	}

	void
	Collection::broadcastCollectionChanged( const DictSlot& slot,
	                                        const ErrorSlot& error ) const {

		aCall<Dict>( connected_,
		             boost::bind( xmmsc_broadcast_collection_changed, conn_ ),
		             slot, error );

	}

	void
	Collection::get( const std::string& name, Namespace nsname,
	                 const CollSlot& slot, const ErrorSlot& error ) const {

		aCall<Coll::Coll>( connected_,
		                   boost::bind( xmmsc_coll_get, conn_,
		                                name.c_str(), nsname ),
		                   slot, error );

	}

	void
	Collection::list( Namespace nsname, const StringListSlot& slot,
	                  const ErrorSlot& error ) const {

		aCall<List<std::string> >( connected_,
		                           boost::bind( xmmsc_coll_list, conn_, nsname ),
		                           slot, error );

	}

	void
	Collection::save( const Coll::Coll& coll, const std::string& name,
		              Namespace nsname, const VoidSlot& slot,
		              const ErrorSlot& error ) const {

		aCall<void>( connected_,
		             boost::bind( xmmsc_coll_save, conn_, coll.coll_,
		                                           name.c_str(), nsname ),
		             slot, error );

	}

	void
	Collection::remove( const std::string& name, Namespace nsname,
	                    const VoidSlot& slot, const ErrorSlot& error ) const {

		aCall<void>( connected_,
		             boost::bind( xmmsc_coll_remove, conn_,
		                                             name.c_str(), nsname ),
		             slot, error );

	}

	void
	Collection::find( unsigned int id, Namespace nsname,
	                  const StringListSlot& slot, const ErrorSlot& error ) const {

		aCall<List<std::string> >( connected_,
		                           boost::bind( xmmsc_coll_find, conn_, id,
		                                                         nsname ),
		                           slot, error );

	}

	void
	Collection::rename( const std::string& from_name,
	                    const std::string& to_name, Namespace nsname,
	                    const VoidSlot& slot, const ErrorSlot& error ) const {

		aCall<void>( connected_,
		             boost::bind( xmmsc_coll_rename, conn_, from_name.c_str(),
		                                             to_name.c_str(), nsname ),
		             slot, error );

	}

	void
	Collection::idlistFromPlaylistFile( const std::string& path,
	                                    const CollSlot& slot,
	                                    const ErrorSlot& error ) const {

		aCall< Coll::Coll >( connected_,
		                     boost::bind( xmmsc_coll_idlist_from_playlist_file,
		                                  conn_, path.c_str() ),
		                     slot, error );

	}

	void
	Collection::queryIds( const Coll::Coll& coll,
	                      const std::list< std::string >& order,
	                      unsigned int limit_len,
	                      unsigned int limit_start,
	                      const UintListSlot& slot,
	                      const ErrorSlot& error ) const {

		std::vector< const char* > corder;
		fillCharArray( order, corder );

		aCall<List<unsigned int> >( connected_,
		                            boost::bind( xmmsc_coll_query_ids, conn_,
		                                         coll.coll_, &corder[0],
		                                         limit_start, limit_len ),
		                            slot, error );

	}

	void
	Collection::queryIds( const Coll::Coll& coll,
	                      const std::list< std::string >& order,
	                      unsigned int limit_len,
	                      const UintListSlot& slot,
	                      const ErrorSlot& error ) const {

		queryIds( coll, order, limit_len, 0, slot, error);

	}

	void
	Collection::queryIds( const Coll::Coll& coll,
	                      const std::list< std::string >& order,
	                      const UintListSlot& slot,
	                      const ErrorSlot& error ) const {

		queryIds( coll, order, 0, 0, slot, error);

	}

	void
	Collection::queryIds( const Coll::Coll& coll,
	                      const UintListSlot& slot,
	                      const ErrorSlot& error ) const {

		queryIds( coll, std::list<std::string>(), 0, 0, slot, error);

	}

	void
	Collection::queryInfos( const Coll::Coll& coll,
	                        const std::list< std::string >& order,
	                        const std::list< std::string >& fetch,
	                        unsigned int limit_len,
	                        unsigned int limit_start,
	                        const std::list< std::string >& group,
	                        const DictListSlot& slot,
	                        const ErrorSlot& error ) const {

		std::vector< const char* > corder, cfetch, cgroup;
		fillCharArray( order, corder );
		fillCharArray( fetch, cfetch );
		fillCharArray( group, cgroup );

		aCall<List<Dict> >( connected_,
		                    boost::bind( xmmsc_coll_query_infos, conn_, coll.coll_,
		                                 &corder[0], limit_start, limit_len,
		                                 &cfetch[0], &cgroup[0] ),
		                    slot, error );

	}

	void
	Collection::queryInfos( const Coll::Coll& coll,
	                        const std::list< std::string >& order,
	                        const std::list< std::string >& fetch,
	                        unsigned int limit_len,
	                        unsigned int limit_start,
	                        const DictListSlot& slot,
	                        const ErrorSlot& error ) const {

		queryInfos( coll, order, fetch, limit_len, limit_start,
		            std::list<std::string>(), slot, error );

	}

	void
	Collection::queryInfos( const Coll::Coll& coll,
	                        const std::list< std::string >& order,
	                        const std::list< std::string >& fetch,
	                        const std::list< std::string >& group,
	                        const DictListSlot& slot,
	                        const ErrorSlot& error ) const {

		queryInfos( coll, order, fetch, 0, 0, group, slot, error );

	}

	void
	Collection::queryInfos( const Coll::Coll& coll,
	                        const std::list< std::string >& order,
	                        const std::list< std::string >& fetch,
	                        const DictListSlot& slot,
	                        const ErrorSlot& error ) const {

		queryInfos( coll, order, fetch, 0, 0, std::list<std::string>(),
		            slot, error );

	}

	void
	Collection::queryInfos( const Coll::Coll& coll,
	                        const std::list< std::string >& order,
	                        const DictListSlot& slot,
	                        const ErrorSlot& error ) const {

		queryInfos( coll, order, std::list<std::string>(), 0, 0,
		            std::list<std::string>(), slot, error );

	}

	void
	Collection::queryInfos( const Coll::Coll& coll,
	                        const DictListSlot& slot,
	                        const ErrorSlot& error ) const {

		queryInfos( coll, std::list<std::string>(), std::list<std::string>(),
		            0, 0, std::list<std::string>(), slot, error );

	}

	Collection::Collection( xmmsc_connection_t*& conn, bool& connected,
	                        MainloopInterface*& ml )
	    : conn_( conn ), connected_( connected ), ml_( ml ) {

	}

}
