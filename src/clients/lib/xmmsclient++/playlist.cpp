#include <xmmsclient/xmmsclient.h>
#include <xmmsclient/xmmsclient++/client.h>
#include <xmmsclient/xmmsclient++/playlist.h>
#include <xmmsclient/xmmsclient++/exceptions.h>
#include <xmmsclient/xmmsclient++/signal.h>
#include <xmmsclient/xmmsclient++/helpers.h>
#include <xmmsclient/xmmsclient++/mainloop.h>

#include <boost/bind.hpp>

#include <string>
#include <list>

namespace Xmms
{

	const std::string Playlist::DEFAULT_PLAYLIST = "_active";


	Playlist::~Playlist()
	{
	}

	void Playlist::addRecursive( const std::string& url,
	                             const std::string& playlist ) const
	{

		vCall( connected_, ml_, 
		       boost::bind( xmmsc_playlist_radd, conn_,
		                    playlist.c_str(), url.c_str() ) );

	}

	void Playlist::addRecursiveEncoded( const std::string& url,
	                                    const std::string& playlist ) const
	{

		vCall( connected_, ml_, 
		       boost::bind( xmmsc_playlist_radd_encoded, conn_,
		                    playlist.c_str(), url.c_str() ) );

	}

	void Playlist::addUrl( const std::string& url,
	                       const std::string& playlist ) const
	{

		vCall( connected_, ml_, 
		       boost::bind( xmmsc_playlist_add_url, conn_,
		                    playlist.c_str(), url.c_str() ) );

	}

	void Playlist::addUrlEncoded( const std::string& url,
	                              const std::string& playlist ) const
	{

		vCall( connected_, ml_, 
		       boost::bind( xmmsc_playlist_add_encoded, conn_,
		                    playlist.c_str(), url.c_str() ) );

	}

	void Playlist::addId( unsigned int id,
	                      const std::string& playlist ) const
	{

		vCall( connected_, ml_, 
		       boost::bind( xmmsc_playlist_add_id, conn_,
		                    playlist.c_str(), id ) );

	}

	void Playlist::clear( const std::string& playlist ) const
	{

		vCall( connected_, ml_, 
		      boost::bind( xmmsc_playlist_clear, conn_, playlist.c_str() ) );

	}

	unsigned int Playlist::currentPos( const std::string& playlist ) const
	{

		xmmsc_result_t* res = 
		    call( connected_, ml_,
		          boost::bind( xmmsc_playlist_current_pos, conn_,
		                       playlist.c_str() ) );

		unsigned int pos = 0;
		xmmsc_result_get_uint( res, &pos );

		xmmsc_result_unref( res );

		return pos;

	}

	void Playlist::insertUrl( int pos, const std::string& url,
	                          const std::string& playlist ) const
	{

		vCall( connected_, ml_,
		       boost::bind( xmmsc_playlist_insert_url, conn_,
		                    playlist.c_str(), pos, url.c_str() ) );

	}

	void Playlist::insertUrlEncoded( int pos, const std::string& url,
	                                 const std::string& playlist ) const
	{

		vCall( connected_, ml_,
		       boost::bind( xmmsc_playlist_insert_encoded, conn_,
		                    playlist.c_str(), pos, url.c_str() ) );

	}


	void Playlist::insertId( int pos, unsigned int id,
	                         const std::string& playlist ) const
	{

		vCall( connected_, ml_,
		       boost::bind( xmmsc_playlist_insert_id, conn_,
		                    playlist.c_str(), pos, id ) );

	}

	List< unsigned int > Playlist::listEntries( const std::string& playlist
	                                          ) const
	{

		xmmsc_result_t* res = 
		    call( connected_, ml_, 
		          boost::bind( xmmsc_playlist_list_entries, conn_,
		                       playlist.c_str() ) );

		List< unsigned int > result( res );

		xmmsc_result_unref( res );

		return result;

	}

	void Playlist::moveEntry( unsigned int curpos, unsigned int newpos,
	                          const std::string& playlist ) const
	{

		vCall( connected_, ml_, 
		       boost::bind( xmmsc_playlist_move_entry, conn_,
		                    playlist.c_str(), curpos, newpos ) );

	}

	void Playlist::removeEntry( unsigned int pos,
	                            const std::string& playlist ) const
	{

		vCall( connected_, ml_, 
		       boost::bind( xmmsc_playlist_remove_entry, conn_,
		                    playlist.c_str(), pos ) );

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

	void Playlist::shuffle( const std::string& playlist ) const
	{

		vCall( connected_, ml_, 
		       boost::bind( xmmsc_playlist_shuffle, conn_, playlist.c_str() ) );

	}

	void Playlist::sort( const std::list<std::string>& properties,
	                     const std::string& playlist ) const
	{

		const char** props = c_stringList( properties );
		vCall( connected_, ml_,
		       boost::bind( xmmsc_playlist_sort, conn_,
		                    playlist.c_str(), props ) );
		delete [] props;

	}

	void
	Playlist::addRecursive( const std::string& url,
	                        const std::string& playlist,
	                        const VoidSlot& slot,
	                        const ErrorSlot& error ) const
	{
		
		aCall<void>( connected_, 
		             boost::bind( xmmsc_playlist_radd, conn_, playlist.c_str(), url.c_str() ),
		             slot, error );

	}

	void
	Playlist::addRecursive( const std::string& url,
	                        const VoidSlot& slot,
	                        const ErrorSlot& error ) const
	{
		addRecursive( url, DEFAULT_PLAYLIST, slot, error );
	}

	void
	Playlist::addRecursiveEncoded( const std::string& url,
	                               const std::string& playlist,
	                               const VoidSlot& slot,
	                               const ErrorSlot& error ) const
	{
		
		aCall<void>( connected_, 
		             boost::bind( xmmsc_playlist_radd_encoded, conn_, playlist.c_str(), url.c_str() ),
		             slot, error );

	}

	void
	Playlist::addRecursiveEncoded( const std::string& url,
	                               const VoidSlot& slot,
	                               const ErrorSlot& error ) const
	{
		addRecursiveEncoded(url, DEFAULT_PLAYLIST, slot, error);
	}

	void
	Playlist::addUrl( const std::string& url,
	                  const std::string& playlist,
	                  const VoidSlot& slot,
	                  const ErrorSlot& error ) const
	{
		
		aCall<void>( connected_, 
		             boost::bind( xmmsc_playlist_add_url, conn_, playlist.c_str(), url.c_str() ),
		             slot, error );

	}

	void
	Playlist::addUrl( const std::string& url,
	                  const VoidSlot& slot,
	                  const ErrorSlot& error ) const
	{
		addUrl( url, DEFAULT_PLAYLIST, slot, error );
	}

	void
	Playlist::addUrlEncoded( const std::string& url,
	                         const std::string& playlist,
	                         const VoidSlot& slot,
	                         const ErrorSlot& error ) const
	{
		
		aCall<void>( connected_, 
		             boost::bind( xmmsc_playlist_add_encoded, conn_, playlist.c_str(), url.c_str() ),
		             slot, error );

	}

	void
	Playlist::addUrlEncoded( const std::string& url,
	                         const VoidSlot& slot,
	                         const ErrorSlot& error ) const
	{
		addUrlEncoded( url, DEFAULT_PLAYLIST, slot, error );
	}

	void
	Playlist::addId( const unsigned int id,
	                 const std::string& playlist,
	                 const VoidSlot& slot,
	                 const ErrorSlot& error ) const
	{

		aCall<void>( connected_,
		             boost::bind( xmmsc_playlist_add_id, conn_, playlist.c_str(), id ),
		             slot, error );

	}

	void
	Playlist::addId( const unsigned int id,
	                 const VoidSlot& slot,
	                 const ErrorSlot& error ) const
	{
		addId( id, DEFAULT_PLAYLIST, slot, error );
	}

	void
	Playlist::clear( const std::string& playlist,
	                 const VoidSlot& slot,
	                 const ErrorSlot& error ) const
	{

		aCall<void>( connected_, boost::bind( xmmsc_playlist_clear, conn_, playlist.c_str() ),
		             slot, error );

	}

	void
	Playlist::clear( const VoidSlot& slot,
	                 const ErrorSlot& error ) const
	{
		clear( DEFAULT_PLAYLIST, slot, error );
	}

	void
	Playlist::currentPos( const std::string& playlist,
					      const UintSlot& slot,
	                      const ErrorSlot& error ) const
	{

		aCall<unsigned int>( connected_, 
		                     boost::bind( xmmsc_playlist_current_pos, conn_, playlist.c_str() ),
		                     slot, error );

	}

	void
	Playlist::currentPos( const UintSlot& slot,
	                      const ErrorSlot& error ) const
	{
		currentPos( DEFAULT_PLAYLIST, slot, error );
	}

	void
	Playlist::insertUrl( int pos, const std::string& url,
	                     const std::string& playlist,
					     const VoidSlot& slot,
					     const ErrorSlot& error ) const
	{

		aCall<void>( connected_,
		             boost::bind( xmmsc_playlist_insert_url, conn_, playlist.c_str(), 
		                          pos, url.c_str() ),
		             slot, error );

	}

	void
	Playlist::insertUrl( int pos, const std::string& url,
					     const VoidSlot& slot,
					     const ErrorSlot& error ) const
	{
		insertUrl( pos, url, DEFAULT_PLAYLIST, slot, error );
	}

	void
	Playlist::insertUrlEncoded( int pos, const std::string& url,
	                            const std::string& playlist,
					            const VoidSlot& slot,
					            const ErrorSlot& error ) const
	{

		aCall<void>( connected_,
		             boost::bind( xmmsc_playlist_insert_encoded, conn_, playlist.c_str(), 
		                          pos, url.c_str() ),
		             slot, error );

	}

	void
	Playlist::insertUrlEncoded( int pos, const std::string& url,
					            const VoidSlot& slot,
					            const ErrorSlot& error ) const
	{
		insertUrlEncoded( pos, url, DEFAULT_PLAYLIST, slot, error );
	}

	void
	Playlist::insertId( int pos, unsigned int id,
	                    const std::string& playlist,
	                    const VoidSlot& slot,
	                    const ErrorSlot& error ) const
	{

		aCall<void>( connected_,
		             boost::bind( xmmsc_playlist_insert_id, conn_, playlist.c_str(), pos, id ),
		             slot, error );

	}

	void
	Playlist::insertId( int pos, unsigned int id,
	                    const VoidSlot& slot,
	                    const ErrorSlot& error ) const
	{
		insertId( pos, id, DEFAULT_PLAYLIST, slot, error );
	}

	void
	Playlist::listEntries( const std::string& playlist,
					       const UintListSlot& slot,
	                       const ErrorSlot& error ) const
	{

		aCall<List<unsigned int> >( connected_, 
		                            boost::bind( xmmsc_playlist_list_entries, conn_, playlist.c_str() ),
		                            slot, error );

	}

	void
	Playlist::listEntries( const UintListSlot& slot,
	                       const ErrorSlot& error ) const
	{
		listEntries( DEFAULT_PLAYLIST, slot, error );
	}

	void
	Playlist::moveEntry( unsigned int curpos, unsigned int newpos,
	                     const std::string& playlist,
	                     const VoidSlot& slot,
	                     const ErrorSlot& error ) const
	{

		aCall<void>( connected_, 
		             boost::bind( xmmsc_playlist_move_entry, conn_, playlist.c_str(), curpos, newpos ),
		             slot, error );

	}

	void
	Playlist::moveEntry( unsigned int curpos, unsigned int newpos,
	                     const VoidSlot& slot,
	                     const ErrorSlot& error ) const
	{
		moveEntry( curpos, newpos, DEFAULT_PLAYLIST, slot, error );
	}

	void
	Playlist::removeEntry( unsigned int pos,
	                       const std::string& playlist,
	                       const VoidSlot& slot,
	                       const ErrorSlot& error ) const
	{

		aCall<void>( connected_,
		             boost::bind( xmmsc_playlist_remove_entry, conn_, playlist.c_str(), pos ),
		             slot, error );

	}

	void
	Playlist::removeEntry( unsigned int pos,
	                       const VoidSlot& slot,
	                       const ErrorSlot& error ) const
	{
		removeEntry( pos, DEFAULT_PLAYLIST, slot, error );
	}

	void
	Playlist::setNext( unsigned int pos,
	                   const UintSlot& slot,
					   const ErrorSlot& error ) const
	{

		aCall<unsigned int>( connected_,
		                     boost::bind( xmmsc_playlist_set_next, conn_, pos ),
		                     slot, error );

	}

	void
	Playlist::setNextRel( signed int pos,
	                      const UintSlot& slot,
	                      const ErrorSlot& error ) const
	{

		aCall<unsigned int>( connected_,
		                     boost::bind( xmmsc_playlist_set_next_rel,
		                                  conn_, pos ),
		                     slot, error );

	}

	void
	Playlist::shuffle( const std::string& playlist,
	                   const VoidSlot& slot,
	                   const ErrorSlot& error ) const
	{

		aCall<void>( connected_, boost::bind( xmmsc_playlist_shuffle, conn_, playlist.c_str() ),
		             slot, error );

	}

	void
	Playlist::shuffle( const VoidSlot& slot,
	                   const ErrorSlot& error ) const
	{
		shuffle( DEFAULT_PLAYLIST, slot, error );
	}

	void
	Playlist::sort( const std::list<std::string>& properties,
	                const std::string& playlist,
	                const VoidSlot& slot,
	                const ErrorSlot& error ) const
	{

		const char** props = c_stringList( properties );
		aCall<void>( connected_, 
		             boost::bind( xmmsc_playlist_sort, conn_, playlist.c_str(), 
		                          props ),
		             slot, error );
		delete [] props;

	}

	void
	Playlist::sort( const std::list<std::string>& properties,
	                const VoidSlot& slot,
	                const ErrorSlot& error ) const
	{
		sort( properties, DEFAULT_PLAYLIST, slot, error );
	}

	void
	Playlist::broadcastChanged( const DictSlot& slot,
	                            const ErrorSlot& error ) const
	{

		aCall<Dict>( connected_,
		             boost::bind( xmmsc_broadcast_playlist_changed, conn_ ),
					 slot, error );

	}

	void
	Playlist::broadcastCurrentPos( const UintSlot& slot,
	                               const ErrorSlot& error ) const
	{

		aCall<unsigned int>( connected_,
		                     boost::bind( xmmsc_broadcast_playlist_current_pos,
		                                  conn_ ),
		                     slot, error );

	}

	Playlist::Playlist( xmmsc_connection_t*& conn, bool& connected,
	                    MainloopInterface*& ml ) :
		conn_( conn ), connected_( connected ), ml_( ml )
	{
	}

}
