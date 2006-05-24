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

namespace Xmms
{

	Medialib::~Medialib()
	{
	}

	std::string Medialib::sqlitePrepareString( const std::string& input ) const
	{
		char* tmp = xmmsc_sqlite_prepare_string( input.c_str() );
		std::string prepstr( tmp );
		free(tmp);

		return prepstr;
	}

	void Medialib::addEntry( const std::string& url ) const
	{

		vCall( connected_, ml_,
		       boost::bind( xmmsc_medialib_add_entry, conn_, url.c_str() ) );

	}

	void Medialib::addToPlaylist( const std::string& query ) const
	{

		vCall( connected_, ml_,
		       boost::bind( xmmsc_medialib_add_to_playlist, 
		                    conn_, query.c_str() ) 
		     );

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
			f = bind( xmmsc_medialib_entry_property_set,
			          conn_, id, key.c_str(), value.c_str() );
		}
		else {
			f = bind( xmmsc_medialib_entry_property_set_with_source,
			          conn_, id, source.c_str(), key.c_str(), value.c_str() );
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

	const std::string Medialib::playlistExport( const std::string& playlist,
	                                            const std::string& mime ) const
	{

		xmmsc_result_t* res = 
		    call( connected_, ml_,
		          boost::bind( xmmsc_medialib_playlist_export,
		                       conn_, playlist.c_str(), mime.c_str() )
		        );

		char* temp = 0;
		xmmsc_result_get_string( res, &temp );

		std::string result( temp );
		xmmsc_result_unref( res );

		return result;

	}

	void Medialib::playlistImport( const std::string& playlist,
	                               const std::string& url ) const
	{

		vCall( connected_, ml_,
		       boost::bind( xmmsc_medialib_playlist_import,
		                    conn_, playlist.c_str(), url.c_str() )
		     );

	}

	List< unsigned int >
	Medialib::playlistList( const std::string& playlist ) const
	{

		xmmsc_result_t* res =
		    call( connected_, ml_,
		          boost::bind( xmmsc_medialib_playlist_list,
		                       conn_, playlist.c_str() )
		        );

		List< unsigned int > result( res );
		xmmsc_result_unref( res );

		return result;

	}

	void Medialib::playlistLoad( const std::string& name ) const
	{

		vCall( connected_, ml_,
		       boost::bind( xmmsc_medialib_playlist_load,
		                    conn_, name.c_str() )
		     );

	}

	void Medialib::playlistRemove( const std::string& playlist ) const
	{

		vCall( connected_, ml_,
		       boost::bind( xmmsc_medialib_playlist_remove,
		                    conn_, playlist.c_str() )
		     );

	}

	void Medialib::playlistSaveCurrent( const std::string& name ) const
	{

		vCall( connected_, ml_,
		       boost::bind( xmmsc_medialib_playlist_save_current,
		                    conn_, name.c_str() )
		     );

	}

	List< std::string > Medialib::playlistsList() const
	{

		xmmsc_result_t* res =
		    call( connected_, ml_,
		          boost::bind( xmmsc_medialib_playlists_list, conn_ )
                );

		List< std::string > result( res );
		xmmsc_result_unref( res );

		return result;

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

	void
	Medialib::addEntry( const std::string& url, 
	                    const std::list< VoidSlot >& slots,
	                    const ErrorSlot& error ) const
	{

		aCall<void>( connected_, 
		             boost::bind( xmmsc_medialib_add_entry, conn_,
		                          url.c_str() ),
		             slots, error );

	}

	void
	Medialib::addToPlaylist( const std::string& query,
	                         const VoidSlot& slot,
	                         const ErrorSlot& error ) const
	{

		aCall<void>( connected_,
		             boost::bind( xmmsc_medialib_add_to_playlist, conn_,
		                          query.c_str() ),
		             slot, error );

	}

	void
	Medialib::addToPlaylist( const std::string& query,
	                         const std::list< VoidSlot >& slots,
	                         const ErrorSlot& error ) const
	{

		aCall<void>( connected_,
		             boost::bind( xmmsc_medialib_add_to_playlist, conn_,
		                          query.c_str() ),
		             slots, error );

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
	                               const std::string& source,
	                               const std::list< VoidSlot >& slots,
	                               const ErrorSlot& error ) const
	{

		using boost::bind;
		aCall<void>( connected_,
		             bind( xmmsc_medialib_entry_property_remove_with_source,
		                   conn_, id, source.c_str(), key.c_str() ),
		             slots, error );

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
	Medialib::entryPropertyRemove( unsigned int id, const std::string& key,
	                               const std::list< VoidSlot >& slots,
	                               const ErrorSlot& error ) const
	{

		aCall<void>( connected_,
		             boost::bind( xmmsc_medialib_entry_property_remove, conn_,
		                          id, key.c_str() ),
		             slots, error );

	}

	void
	Medialib::entryPropertySet( unsigned int id, const std::string& key,
	                            const std::string& value,
	                            const std::string& source,
	                            const VoidSlot& slot,
	                            const ErrorSlot& error ) const
	{

		aCall<void>( connected_,
		             boost::bind( xmmsc_medialib_entry_property_set_with_source,
		                          conn_, id, source.c_str(), key.c_str(),
		                          value.c_str() ),
		             slot, error );

	}

	void
	Medialib::entryPropertySet( unsigned int id, const std::string& key,
	                            const std::string& value,
	                            const std::string& source,
	                            const std::list< VoidSlot >& slots,
	                            const ErrorSlot& error ) const
	{

		aCall<void>( connected_,
		             boost::bind( xmmsc_medialib_entry_property_set_with_source,
		                          conn_, id, source.c_str(), key.c_str(),
		                          value.c_str() ),
		             slots, error );

	}

	void
	Medialib::entryPropertySet( unsigned int id, const std::string& key,
	                            const std::string& value,
	                            const VoidSlot& slot,
	                            const ErrorSlot& error ) const
	{

		aCall<void>( connected_,
		             boost::bind( xmmsc_medialib_entry_property_set, conn_,
		                          id, key.c_str(), value.c_str() ),
		             slot, error );

	}

	void
	Medialib::entryPropertySet( unsigned int id, const std::string& key,
	                            const std::string& value,
	                            const std::list< VoidSlot >& slots,
	                            const ErrorSlot& error ) const
	{

		aCall<void>( connected_,
		             boost::bind( xmmsc_medialib_entry_property_set, conn_,
		                          id, key.c_str(), value.c_str() ),
		             slots, error );

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
	Medialib::getID( const std::string& url, const std::list< UintSlot >& slots,
	                 const ErrorSlot& error ) const
	{

		aCall<unsigned int>( connected_,
		                     boost::bind( xmmsc_medialib_get_id, conn_,
		                                  url.c_str() ),
		                     slots, error );

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
	Medialib::getInfo( unsigned int id, const std::list< PropDictSlot >& slots,
	                   const ErrorSlot& error ) const
	{

		aCall<PropDict>( connected_,
		                 boost::bind( xmmsc_medialib_get_info, conn_, id ),
		                 slots, error );

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
	Medialib::pathImport( const std::string& path,
	                      const std::list< VoidSlot >& slots,
	                      const ErrorSlot& error ) const
	{

		aCall<void>( connected_,
		             boost::bind( xmmsc_medialib_path_import, conn_,
		                          path.c_str() ),
		             slots, error );

	}

	void
	Medialib::playlistExport( const std::string& playlist,
	                          const std::string& mime,
	                          const StringSlot& slot,
	                          const ErrorSlot& error ) const
	{

		aCall<std::string>( connected_,
		                    boost::bind( xmmsc_medialib_playlist_export, conn_,
		                                 playlist.c_str(), mime.c_str() ),
		                    slot, error );

	}

	void
	Medialib::playlistExport( const std::string& playlist,
	                          const std::string& mime,
	                          const std::list< StringSlot >& slots,
	                          const ErrorSlot& error ) const
	{

		aCall<std::string>( connected_,
		                    boost::bind( xmmsc_medialib_playlist_export, conn_,
		                                 playlist.c_str(), mime.c_str() ),
		                    slots, error );

	}

	void
	Medialib::playlistImport( const std::string& playlist,
	                          const std::string& url,
	                          const VoidSlot& slot,
	                          const ErrorSlot& error ) const
	{

		aCall<void>( connected_,
		             boost::bind( xmmsc_medialib_playlist_import, conn_,
		                          playlist.c_str(), url.c_str() ),
		             slot, error );

	}

	void
	Medialib::playlistImport( const std::string& playlist,
	                          const std::string& url,
	                          const std::list< VoidSlot >& slots,
	                          const ErrorSlot& error ) const
	{

		aCall<void>( connected_,
		             boost::bind( xmmsc_medialib_playlist_import, conn_,
		                          playlist.c_str(), url.c_str() ),
		             slots, error );

	}

	void
	Medialib::playlistList( const std::string& playlist,
	                        const UintListSlot& slot,
	                        const ErrorSlot& error ) const
	{

		aCall<List<unsigned int> >( connected_,
		                            boost::bind( xmmsc_medialib_playlist_list,
		                                         conn_, playlist.c_str() ),
		                            slot, error );

	}

	void
	Medialib::playlistList( const std::string& playlist,
	                        const std::list< UintListSlot >& slots,
	                        const ErrorSlot& error ) const
	{

		aCall<List<unsigned int> >( connected_,
		                            boost::bind( xmmsc_medialib_playlist_list,
		                                         conn_, playlist.c_str() ),
		                            slots, error );

	}

	void
	Medialib::playlistLoad( const std::string& name,
	                        const VoidSlot& slot, const ErrorSlot& error ) const
	{

		aCall<void>( connected_,
		             boost::bind( xmmsc_medialib_playlist_load, conn_, 
		                          name.c_str() ),
		             slot, error );

	}

	void
	Medialib::playlistLoad( const std::string& name,
	                        const std::list< VoidSlot >& slots,
	                        const ErrorSlot& error ) const
	{

		aCall<void>( connected_,
		             boost::bind( xmmsc_medialib_playlist_load, conn_, 
		                          name.c_str() ),
		             slots, error );

	}

	void
	Medialib::playlistRemove( const std::string& playlist,
	                          const VoidSlot& slot,
	                          const ErrorSlot& error ) const
	{

		aCall<void>( connected_,
		             boost::bind( xmmsc_medialib_playlist_remove, conn_,
		                          playlist.c_str() ),
		             slot, error );

	}

	void
	Medialib::playlistRemove( const std::string& playlist,
	                          const std::list< VoidSlot >& slots,
	                          const ErrorSlot& error ) const
	{

		aCall<void>( connected_,
		             boost::bind( xmmsc_medialib_playlist_remove, conn_,
		                          playlist.c_str() ),
		             slots, error );

	}

	void
	Medialib::playlistSaveCurrent( const std::string& name,
	                               const VoidSlot& slot,
	                               const ErrorSlot& error ) const
	{

		aCall<void>( connected_,
		             boost::bind( xmmsc_medialib_playlist_save_current, conn_,
		                          name.c_str() ),
		             slot, error );

	}

	void
	Medialib::playlistSaveCurrent( const std::string& name,
	                               const std::list< VoidSlot >& slots,
	                               const ErrorSlot& error ) const
	{

		aCall<void>( connected_,
		             boost::bind( xmmsc_medialib_playlist_save_current, conn_,
		                          name.c_str() ),
		             slots, error );

	}

	void
	Medialib::playlistsList( const StringListSlot& slot,
	                         const ErrorSlot& error ) const
	{

		aCall<List<std::string> >( connected_,
		                           boost::bind( xmmsc_medialib_playlists_list,
		                                        conn_ ),
		                           slot, error );

	}

	void
	Medialib::playlistsList( const std::list< StringListSlot >& slots,
	                         const ErrorSlot& error ) const
	{

		aCall<List<std::string> >( connected_,
		                           boost::bind( xmmsc_medialib_playlists_list,
		                                        conn_ ),
		                           slots, error );

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
	Medialib::rehash( unsigned int id, const std::list< VoidSlot >& slots,
	                  const ErrorSlot& error ) const
	{

		aCall<void>( connected_,
		             boost::bind( xmmsc_medialib_rehash, conn_, id ),
		             slots, error );

	}

	void
	Medialib::rehash( const VoidSlot& slot, const ErrorSlot& error ) const
	{

		rehash( 0, slot, error );

	}

	void
	Medialib::rehash( const std::list< VoidSlot >& slots,
	                  const ErrorSlot& error ) const
	{

		rehash( 0, slots, error );

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
	Medialib::removeEntry( unsigned int id, const std::list< VoidSlot >& slots,
	                       const ErrorSlot& error ) const
	{

		aCall<void>( connected_,
		             boost::bind( xmmsc_medialib_remove_entry, conn_, id ),
		             slots, error );

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
	Medialib::select( const std::string& query,
	                  const std::list< DictListSlot >& slots,
	                  const ErrorSlot& error ) const
	{

		aCall<List<Dict> >( connected_,
		                    boost::bind( xmmsc_medialib_select, conn_,
		                                 query.c_str() ),
		                    slots, error );

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
	Medialib::broadcastEntryAdded( const std::list< UintSlot >& slots,
	                               const ErrorSlot& error ) const
	{

		aCall<unsigned int>( connected_,
		                     boost::bind( xmmsc_broadcast_medialib_entry_added,
		                                  conn_ ),
		                     slots, error );

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

	void
	Medialib::broadcastEntryChanged( const std::list< UintSlot >& slots,
	                                 const ErrorSlot& error ) const
	{

		using boost::bind;
		aCall<unsigned int>( connected_,
		                     bind( xmmsc_broadcast_medialib_entry_changed,
		                           conn_ ),
		                     slots, error );

	}

	void
	Medialib::broadcastPlaylistLoaded( const StringSlot& slot,
	                                   const ErrorSlot& error ) const
	{

		using boost::bind;
		aCall<std::string>( connected_,
		                    bind( xmmsc_broadcast_medialib_playlist_loaded,
		                          conn_ ),
		                    slot, error );

	}

	void
	Medialib::broadcastPlaylistLoaded( const std::list< StringSlot >& slots,
	                                   const ErrorSlot& error ) const
	{

		using boost::bind;
		aCall<std::string>( connected_,
		                    bind( xmmsc_broadcast_medialib_playlist_loaded,
		                          conn_ ),
		                    slots, error );

	}

	Medialib::Medialib( xmmsc_connection_t*& conn, bool& connected,
	                    MainloopInterface*& ml ) :
		conn_( conn ), connected_( connected ), ml_( ml )
	{
	}

}

