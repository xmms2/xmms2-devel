/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2006 XMMS2 Team
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

#include <boost/function.hpp>
#include <boost/bind.hpp>

#include <string>
#include <list>
#include <vector>

namespace Xmms
{

	Medialib::~Medialib()
	{
	}

	void Medialib::addEntry( const std::string& url ) const
	{

		vCall( connected_, ml_,
		       boost::bind( xmmsc_medialib_add_entry, conn_, url.c_str() ) );

	}

	void Medialib::addEntry( const std::string& url,
	                         const std::list< std::string >& args ) const
	{

		std::vector< const char* > cargs;
		fillCharArray( args, cargs );

		vCall( connected_, ml_,
		       boost::bind( xmmsc_medialib_add_entry_args, conn_,
		                    url.c_str(), args.size(), &cargs[0] ) );

	}

	void Medialib::addEntryEncoded( const std::string& url ) const
	{

		vCall( connected_, ml_,
		       boost::bind( xmmsc_medialib_add_entry_encoded, conn_, url.c_str() ) );

	}

	void Medialib::entryPropertyRemove( unsigned int id,
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

		vCall( connected_, ml_, f );

	}

	void Medialib::entryPropertySet( unsigned int id,
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

		vCall( connected_, ml_, f );

	}

	void Medialib::entryPropertySet( unsigned int id,
	                                 const std::string& key,
	                                 const int value,
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

		vCall( connected_, ml_, f );

	}


	unsigned int Medialib::getID( const std::string& url ) const
	{

		xmmsc_result_t* res = call( connected_, ml_,
		                            boost::bind( xmmsc_medialib_get_id,
		                                         conn_, url.c_str() )
		                          );
		unsigned int id = 0;
		xmmsc_result_get_uint( res, &id );
		xmmsc_result_unref( res );

		return id;

	}

	PropDict Medialib::getInfo( unsigned int id ) const
	{

		xmmsc_result_t* res = call( connected_, ml_,
		                            boost::bind( xmmsc_medialib_get_info,
		                                         conn_, id )
		                          );
		PropDict result( res );
		xmmsc_result_unref( res );

		return result;

	}

	void Medialib::pathImport( const std::string& path ) const
	{

		vCall( connected_, ml_,
		       boost::bind( xmmsc_medialib_path_import, conn_, path.c_str() )
		     );

	}

	void Medialib::pathImportEncoded( const std::string& path ) const
	{

		vCall( connected_, ml_,
		       boost::bind( xmmsc_medialib_path_import_encoded, conn_, path.c_str() )
		     );

	}

	void Medialib::rehash( unsigned int id ) const
	{

		vCall( connected_, ml_,
		       boost::bind( xmmsc_medialib_rehash, conn_, id )
		     );

	}

	void Medialib::removeEntry( unsigned int id ) const
	{

		vCall( connected_, ml_,
		       boost::bind( xmmsc_medialib_remove_entry, conn_, id )
		     );

	}

	List< Dict > Medialib::select( const std::string& query ) const
	{

		xmmsc_result_t* res =
		    call( connected_, ml_,
		          boost::bind( xmmsc_medialib_select, conn_, query.c_str() )
		        );

		List< Dict > result( res );
		xmmsc_result_unref( res );

		return result;

	}

	void
	Medialib::addEntry( const std::string& url, const VoidSlot& slot,
	                    const ErrorSlot& error ) const
	{

		aCall<void>( connected_, 
		             boost::bind( xmmsc_medialib_add_entry, conn_,
		                          url.c_str() ),
		             slot, error );

	}

	void Medialib::addEntry( const std::string& url,
	                         const std::list< std::string >& args,
	                         const VoidSlot& slot, const ErrorSlot& error
	                       ) const {

		std::vector< const char* > cargs( args.size() );

		std::vector< const char* >::size_type i = 0;
		for( std::list< std::string >::const_iterator it = args.begin();
		     it != args.end(); ++it ) {

			cargs[i++] = it->c_str();

		}

		aCall<void>( connected_,
		             boost::bind( xmmsc_medialib_add_entry_args, conn_,
		                          url.c_str(), args.size(), &cargs[0] ),
		             slot, error );

	}

	void
	Medialib::addEntryEncoded( const std::string& url, const VoidSlot& slot,
	                           const ErrorSlot& error ) const
	{

		aCall<void>( connected_, 
		             boost::bind( xmmsc_medialib_add_entry_encoded, conn_,
		                          url.c_str() ),
		             slot, error );

	}

	void
	Medialib::entryPropertyRemove( unsigned int id, const std::string& key,
	                               const std::string& source,
	                               const VoidSlot& slot,
	                               const ErrorSlot& error ) const
	{

		using boost::bind;
		aCall<void>( connected_,
		             bind( xmmsc_medialib_entry_property_remove_with_source,
		                   conn_, id, source.c_str(), key.c_str() ),
		             slot, error );

	}

	void
	Medialib::entryPropertyRemove( unsigned int id, const std::string& key,
	                               const VoidSlot& slot,
	                               const ErrorSlot& error ) const
	{

		aCall<void>( connected_,
		             boost::bind( xmmsc_medialib_entry_property_remove, conn_,
		                          id, key.c_str() ),
		             slot, error );

	}

	void
	Medialib::entryPropertySet( unsigned int id, const std::string& key,
	                            const std::string& value,
	                            const std::string& source,
	                            const VoidSlot& slot,
	                            const ErrorSlot& error ) const
	{

		aCall<void>( connected_,
		             boost::bind( xmmsc_medialib_entry_property_set_str_with_source,
		                          conn_, id, source.c_str(), key.c_str(),
		                          value.c_str() ),
		             slot, error );

	}

	void
	Medialib::entryPropertySet( unsigned int id, const std::string& key,
	                            const std::string& value,
	                            const VoidSlot& slot,
	                            const ErrorSlot& error ) const
	{

		aCall<void>( connected_,
		             boost::bind( xmmsc_medialib_entry_property_set_str, conn_,
		                          id, key.c_str(), value.c_str() ),
		             slot, error );

	}

	void
	Medialib::entryPropertySet( unsigned int id, const std::string& key,
	                            int value,
	                            const std::string& source,
	                            const VoidSlot& slot,
	                            const ErrorSlot& error ) const
	{

		aCall<void>( connected_,
		             boost::bind( xmmsc_medialib_entry_property_set_int_with_source,
		                          conn_, id, source.c_str(), key.c_str(),
		                          value ),
		             slot, error );

	}

	void
	Medialib::entryPropertySet( unsigned int id, const std::string& key,
	                            int value,
	                            const VoidSlot& slot,
	                            const ErrorSlot& error ) const
	{

		aCall<void>( connected_,
		             boost::bind( xmmsc_medialib_entry_property_set_int, conn_,
		                          id, key.c_str(), value ),
		             slot, error );

	}

	void
	Medialib::getID( const std::string& url, const UintSlot& slot,
	                 const ErrorSlot& error ) const
	{

		aCall<unsigned int>( connected_,
		                     boost::bind( xmmsc_medialib_get_id, conn_,
		                                  url.c_str() ),
		                     slot, error );

	}

	void
	Medialib::getInfo( unsigned int id, const PropDictSlot& slot,
	                   const ErrorSlot& error ) const
	{

		aCall<PropDict>( connected_,
		                 boost::bind( xmmsc_medialib_get_info, conn_, id ),
		                 slot, error );

	}

	void
	Medialib::pathImport( const std::string& path, const VoidSlot& slot,
	                      const ErrorSlot& error ) const
	{

		aCall<void>( connected_,
		             boost::bind( xmmsc_medialib_path_import, conn_,
		                          path.c_str() ),
		             slot, error );

	}

	void
	Medialib::pathImportEncoded( const std::string& path, const VoidSlot& slot,
	                             const ErrorSlot& error ) const
	{

		aCall<void>( connected_,
		             boost::bind( xmmsc_medialib_path_import_encoded, conn_,
		                          path.c_str() ),
		             slot, error );

	}

	void
	Medialib::rehash( unsigned int id, const VoidSlot& slot,
	                  const ErrorSlot& error ) const
	{

		aCall<void>( connected_,
		             boost::bind( xmmsc_medialib_rehash, conn_, id ),
		             slot, error );

	}

	void
	Medialib::rehash( const VoidSlot& slot, const ErrorSlot& error ) const
	{

		rehash( 0, slot, error );

	}

	void
	Medialib::removeEntry( unsigned int id, const VoidSlot& slot,
	                       const ErrorSlot& error ) const
	{

		aCall<void>( connected_,
		             boost::bind( xmmsc_medialib_remove_entry, conn_, id ),
		             slot, error );

	}

	void
	Medialib::select( const std::string& query, const DictListSlot& slot,
	                  const ErrorSlot& error ) const
	{

		aCall<List<Dict> >( connected_,
		                    boost::bind( xmmsc_medialib_select, conn_,
		                                 query.c_str() ),
		                    slot, error );

	}

	void
	Medialib::broadcastEntryAdded( const UintSlot& slot,
	                               const ErrorSlot& error ) const
	{

		aCall<unsigned int>( connected_,
		                     boost::bind( xmmsc_broadcast_medialib_entry_added,
		                                  conn_ ),
		                     slot, error );

	}

	void
	Medialib::broadcastEntryChanged( const UintSlot& slot,
	                                 const ErrorSlot& error ) const
	{

		using boost::bind;
		aCall<unsigned int>( connected_,
		                     bind( xmmsc_broadcast_medialib_entry_changed,
		                           conn_ ),
		                     slot, error );

	}

	Medialib::Medialib( xmmsc_connection_t*& conn, bool& connected,
	                    MainloopInterface*& ml ) :
		conn_( conn ), connected_( connected ), ml_( ml )
	{
	}

}

