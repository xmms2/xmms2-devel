#include <xmmsclient/xmmsclient.h>
#include <xmmsclient/xmmsclient++/client.h>
#include <xmmsclient/xmmsclient++/playlist.h>
#include <xmmsclient/xmmsclient++/exceptions.h>
#include <xmmsclient/xmmsclient++/helpers.h>
#include <xmmsclient/xmmsclient++/mainloop.h>

#include <string>

namespace Xmms
{

	Playlist::~Playlist()
	{
	}

	void Playlist::add( const std::string& url ) const
	{

		Assert( connected_ );
		Assert( ml_ );

		xmmsc_result_t* res = xmmsc_playlist_add( conn_, url.c_str() );
		xmmsc_result_wait( res );

		Assert( res );

		xmmsc_result_unref( res );

	}

	void Playlist::add( unsigned int id ) const
	{

		Assert( connected_ );
		Assert( ml_ );

		xmmsc_result_t* res = xmmsc_playlist_add_id( conn_, id );
		xmmsc_result_wait( res );

		Assert( res );

		xmmsc_result_unref( res );

	}

	void Playlist::clear() const
	{

		Assert( connected_ );
		Assert( ml_ );

		xmmsc_result_t* res = xmmsc_playlist_clear( conn_ );
		xmmsc_result_wait( res );

		Assert( res );

		xmmsc_result_unref( res );

	}

	unsigned int Playlist::currentPos() const
	{

		Assert( connected_ );
		Assert( ml_ );

		xmmsc_result_t* res = xmmsc_playlist_current_pos( conn_ );
		xmmsc_result_wait( res );

		Assert( res );

		unsigned int pos = 0;
		xmmsc_result_get_uint( res, &pos );

		return pos;

	}

	void Playlist::insert( int pos, const std::string& url ) const
	{

		Assert( connected_ );
		Assert( ml_ );

		xmmsc_result_t* res = xmmsc_playlist_insert( conn_, pos, url.c_str() );
		xmmsc_result_wait( res );

		Assert( res );

		xmmsc_result_unref( res );

	}

	void Playlist::insert( int pos, unsigned int id ) const
	{

		Assert( connected_ );
		Assert( ml_ );

		xmmsc_result_t* res = xmmsc_playlist_insert_id( conn_, pos, id );
		xmmsc_result_wait( res );

		Assert( res );

		xmmsc_result_unref( res );

	}

	List< unsigned int > Playlist::list() const
	{

		Assert( connected_ );
		Assert( ml_ );

		xmmsc_result_t* res = xmmsc_playlist_list( conn_ );
		xmmsc_result_wait( res );

		Assert( res );

		List< unsigned int > result( res );

		xmmsc_result_unref( res );

		return result;

	}

	void Playlist::move( unsigned int curpos, unsigned int newpos ) const
	{

		Assert( connected_ );
		Assert( ml_ );

		xmmsc_result_t* res = xmmsc_playlist_move( conn_, curpos, newpos );
		xmmsc_result_wait( res );

		Assert( res );

		xmmsc_result_unref( res );

	}

	void Playlist::remove( unsigned int pos ) const
	{

		Assert( connected_ );
		Assert( ml_ );

		xmmsc_result_t* res = xmmsc_playlist_remove( conn_, pos );
		xmmsc_result_wait( res );

		Assert( res );

		xmmsc_result_unref( res );

	}

	unsigned int Playlist::setNext( unsigned int pos ) const
	{

		Assert( connected_ );
		Assert( ml_ );

		xmmsc_result_t* res = xmmsc_playlist_set_next( conn_, pos );
		xmmsc_result_wait( res );

		Assert( res );

		unsigned int result = 0;
		xmmsc_result_get_uint( res, &result );

		xmmsc_result_unref( res );

		return result;

	}

	unsigned int Playlist::setNextRel( signed int pos ) const
	{
		
		Assert( connected_ );
		Assert( ml_ );

		xmmsc_result_t* res = xmmsc_playlist_set_next_rel( conn_, pos );
		xmmsc_result_wait( res );

		Assert( res );

		unsigned int result = 0;
		xmmsc_result_get_uint( res, &result );

		xmmsc_result_unref( res );

		return result;

	}

	void Playlist::shuffle() const
	{

		Assert( connected_ );
		Assert( ml_ );

		xmmsc_result_t* res = xmmsc_playlist_shuffle( conn_ );
		xmmsc_result_wait( res );

		Assert( res );

		xmmsc_result_unref( res );

	}

	void Playlist::sort( const std::string& property ) const
	{

		Assert( connected_ );
		Assert( ml_ );

		xmmsc_result_t* res = xmmsc_playlist_sort( conn_, property.c_str() );
		xmmsc_result_wait( res );

		Assert( res );
		
		xmmsc_result_unref( res );

	}

	Playlist::Playlist( xmmsc_connection_t*& conn, bool& connected,
	                    MainLoop*& ml ) :
		conn_( conn ), connected_( connected ), ml_( ml )
	{
	}

}
