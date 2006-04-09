#include <xmmsclient/xmmsclient.h>
#include <xmmsclient/xmmsclient++/medialib.h>
#include <xmmsclient/xmmsclient++/mainloop.h>
#include <xmmsclient/xmmsclient++/dict.h>
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

	Medialib::Medialib( xmmsc_connection_t*& conn, bool& connected,
	                    MainLoop*& ml ) :
		conn_( conn ), connected_( connected ), ml_( ml )
	{
	}

}

