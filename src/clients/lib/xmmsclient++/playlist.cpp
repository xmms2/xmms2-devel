#include <xmmsclient/xmmsclient.h>
#include <xmmsclient/xmmsclient++/client.h>
#include <xmmsclient/xmmsclient++/playlist.h>
#include <xmmsclient/xmmsclient++/exceptions.h>
#include <xmmsclient/xmmsclient++/helpers.h>
#include <xmmsclient/xmmsclient++/mainloop.h>

#include <boost/bind.hpp>

#include <string>

namespace Xmms
{

	Playlist::~Playlist()
	{
	}

	void Playlist::add( const std::string& url ) const
	{

		vCall( connected_, ml_, 
		      boost::bind( xmmsc_playlist_add, conn_, url.c_str() ) );

	}

	void Playlist::add( unsigned int id ) const
	{

		vCall( connected_, ml_, 
		      boost::bind( xmmsc_playlist_add_id, conn_, id ) );

	}

	void Playlist::clear() const
	{

		vCall( connected_, ml_, 
		      boost::bind( xmmsc_playlist_clear, conn_ ) );

	}

	unsigned int Playlist::currentPos() const
	{

		xmmsc_result_t* res = 
		    call( connected_, ml_,
		          boost::bind( xmmsc_playlist_current_pos, conn_ ) );

		unsigned int pos = 0;
		xmmsc_result_get_uint( res, &pos );

		xmmsc_result_unref( res );

		return pos;

	}

	void Playlist::insert( int pos, const std::string& url ) const
	{

		vCall( connected_, ml_,
		      boost::bind( xmmsc_playlist_insert, conn_, pos, url.c_str() ) );

	}

	void Playlist::insert( int pos, unsigned int id ) const
	{

		vCall( connected_, ml_,
		      boost::bind( xmmsc_playlist_insert_id, conn_, pos, id ) );

	}

	List< unsigned int > Playlist::list() const
	{

		xmmsc_result_t* res = 
		    call( connected_, ml_, 
		          boost::bind( xmmsc_playlist_list, conn_ ) );

		List< unsigned int > result( res );

		xmmsc_result_unref( res );

		return result;

	}

	void Playlist::move( unsigned int curpos, unsigned int newpos ) const
	{

		vCall( connected_, ml_, 
		      boost::bind( xmmsc_playlist_move, conn_, curpos, newpos ) );

	}

	void Playlist::remove( unsigned int pos ) const
	{

		vCall( connected_, ml_, 
		      boost::bind( xmmsc_playlist_remove, conn_, pos ) );

	}

	unsigned int Playlist::setNext( unsigned int pos ) const
	{

		xmmsc_result_t* res = 
		    call( connected_, ml_,
		          boost::bind( xmmsc_playlist_set_next, conn_, pos ) );

		unsigned int result = 0;
		xmmsc_result_get_uint( res, &result );

		xmmsc_result_unref( res );

		return result;

	}

	unsigned int Playlist::setNextRel( signed int pos ) const
	{

		xmmsc_result_t* res = 
		    call( connected_, ml_, 
		          boost::bind( xmmsc_playlist_set_next_rel, conn_, pos ) );

		unsigned int result = 0;
		xmmsc_result_get_uint( res, &result );

		xmmsc_result_unref( res );

		return result;

	}

	void Playlist::shuffle() const
	{

		vCall( connected_, ml_, 
		      boost::bind( xmmsc_playlist_shuffle, conn_ ) );

	}

	void Playlist::sort( const std::string& property ) const
	{

		vCall( connected_, ml_,
		      boost::bind( xmmsc_playlist_sort, conn_, property.c_str() ) );

	}

	Playlist::Playlist( xmmsc_connection_t*& conn, bool& connected,
	                    MainLoop*& ml ) :
		conn_( conn ), connected_( connected ), ml_( ml )
	{
	}

}
