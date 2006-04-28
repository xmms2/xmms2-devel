#include <xmmsclient/xmmsclient.h>
#include <xmmsclient/xmmsclient++/client.h>
#include <xmmsclient/xmmsclient++/playlist.h>
#include <xmmsclient/xmmsclient++/exceptions.h>
#include <xmmsclient/xmmsclient++/signal.h>
#include <xmmsclient/xmmsclient++/helpers.h>
#include <xmmsclient/xmmsclient++/mainloop.h>

#include <boost/bind.hpp>

#include <string>

namespace Xmms
{

	Playlist::~Playlist()
	{
	}

	void Playlist::addUrl( const std::string& url ) const
	{

		vCall( connected_, ml_, 
		      boost::bind( xmmsc_playlist_add, conn_, url.c_str() ) );

	}

	void Playlist::addId( unsigned int id ) const
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

	void Playlist::insertUrl( int pos, const std::string& url ) const
	{

		vCall( connected_, ml_,
		      boost::bind( xmmsc_playlist_insert, conn_, pos, url.c_str() ) );

	}

	void Playlist::insertId( int pos, unsigned int id ) const
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

	void
	Playlist::addUrl( const std::string& url,
	                  const VoidSlot& slot,
	                  const ErrorSlot& error ) const
	{
		
		aCall<void>( connected_, 
		             boost::bind( xmmsc_playlist_add, conn_, url.c_str() ),
		             slot, error );

	}

	void
	Playlist::addUrl( const std::string& url,
	                  const std::list< VoidSlot >& slots,
	                  const ErrorSlot& error ) const
	{

		aCall<void>( connected_,
		             boost::bind( xmmsc_playlist_add, conn_, url.c_str() ),
		             slots, error );

	}

	void
	Playlist::addId( const unsigned int id,
	                 const VoidSlot& slot,
	                 const ErrorSlot& error ) const
	{

		aCall<void>( connected_,
		             boost::bind( xmmsc_playlist_add_id, conn_, id ),
		             slot, error );

	}

	void
	Playlist::addId( const unsigned int id,
	                 const std::list< VoidSlot >& slots,
	                 const ErrorSlot& error ) const
	{

		aCall<void>( connected_,
		             boost::bind( xmmsc_playlist_add_id, conn_, id ),
		             slots, error );

	}

	void
	Playlist::clear( const VoidSlot& slot,
	                 const ErrorSlot& error ) const
	{

		aCall<void>( connected_, boost::bind( xmmsc_playlist_clear, conn_ ),
		             slot, error );

	}

	void
	Playlist::clear( const std::list< VoidSlot >& slots,
	                 const ErrorSlot& error ) const
	{

		aCall<void>( connected_, boost::bind( xmmsc_playlist_clear, conn_ ),
		             slots, error );

	}

	void
	Playlist::currentPos( const UintSlot& slot,
	                      const ErrorSlot& error ) const
	{

		aCall<unsigned int>( connected_, 
		                     boost::bind( xmmsc_playlist_current_pos, conn_ ),
		                     slot, error );

	}

	void
	Playlist::currentPos( const std::list< UintSlot >& slots,
	                      const ErrorSlot& error ) const
	{

		aCall<unsigned int>( connected_, 
		                     boost::bind( xmmsc_playlist_current_pos, conn_ ),
		                     slots, error );

	}

	void
	Playlist::insertUrl( int pos, const std::string& url,
	                     const VoidSlot& slot,
					     const ErrorSlot& error ) const
	{

		aCall<void>( connected_,
		             boost::bind( xmmsc_playlist_insert, conn_, 
		                          pos, url.c_str() ),
		             slot, error );

	}

	void
	Playlist::insertUrl( int pos, const std::string& url,
	                  const std::list< VoidSlot >& slots,
					  const ErrorSlot& error ) const
	{

		aCall<void>( connected_,
		             boost::bind( xmmsc_playlist_insert, conn_, 
		                          pos, url.c_str() ),
		             slots, error );

	}

	void
	Playlist::insertId( int pos, unsigned int id,
	                    const VoidSlot& slot,
	                    const ErrorSlot& error ) const
	{

		aCall<void>( connected_,
		             boost::bind( xmmsc_playlist_insert_id, conn_, pos, id ),
		             slot, error );

	}

	void
	Playlist::insertId( int pos, unsigned int id,
	                    const std::list< VoidSlot >& slots,
	                    const ErrorSlot& error ) const
	{

		aCall<void>( connected_,
		             boost::bind( xmmsc_playlist_insert_id, conn_, pos, id ),
		             slots, error );

	}

	void
	Playlist::list( const UintListSlot& slot,
	                const ErrorSlot& error ) const
	{

		aCall<List<unsigned int> >( connected_, 
		                            boost::bind( xmmsc_playlist_list, conn_ ),
		                            slot, error );

	}

	void
	Playlist::list( const std::list< UintListSlot >& slots,
	                const ErrorSlot& error ) const
	{

		aCall<List<unsigned int> >( connected_, 
		                            boost::bind( xmmsc_playlist_list, conn_ ),
		                            slots, error );

	}

	void
	Playlist::move( unsigned int curpos, unsigned int newpos,
	                const VoidSlot& slot,
	                const ErrorSlot& error ) const
	{

		aCall<void>( connected_, 
		             boost::bind( xmmsc_playlist_move, conn_, curpos, newpos ),
		             slot, error );

	}

	void
	Playlist::move( unsigned int curpos, unsigned int newpos,
	                const std::list< VoidSlot >& slots,
	                const ErrorSlot& error ) const
	{

		aCall<void>( connected_, 
		             boost::bind( xmmsc_playlist_move, conn_, curpos, newpos ),
		             slots, error );

	}

	void
	Playlist::remove( unsigned int pos,
	                  const VoidSlot& slot,
	                  const ErrorSlot& error ) const
	{

		aCall<void>( connected_,
		             boost::bind( xmmsc_playlist_remove, conn_, pos ),
		             slot, error );

	}

	void
	Playlist::remove( unsigned int pos,
	                  const std::list< VoidSlot >& slots,
	                  const ErrorSlot& error ) const
	{

		aCall<void>( connected_,
		             boost::bind( xmmsc_playlist_remove, conn_, pos ),
		             slots, error );

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
	Playlist::setNext( unsigned int pos,
	                   const std::list< UintSlot >& slots,
					   const ErrorSlot& error ) const
	{

		aCall<unsigned int>( connected_,
		                     boost::bind( xmmsc_playlist_set_next, conn_, pos ),
		                     slots, error );

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
	Playlist::setNextRel( signed int pos,
	                      const std::list< UintSlot >& slots,
	                      const ErrorSlot& error ) const
	{

		aCall<unsigned int>( connected_,
		                     boost::bind( xmmsc_playlist_set_next_rel,
		                                  conn_, pos ),
		                     slots, error );

	}

	void
	Playlist::shuffle( const VoidSlot& slot,
	                   const ErrorSlot& error ) const
	{

		aCall<void>( connected_, boost::bind( xmmsc_playlist_shuffle, conn_ ),
		             slot, error );

	}

	void
	Playlist::shuffle( const std::list< VoidSlot >& slots,
	                   const ErrorSlot& error ) const
	{

		aCall<void>( connected_, boost::bind( xmmsc_playlist_shuffle, conn_ ),
		             slots, error );

	}

	void
	Playlist::sort( const std::string& property,
	                const VoidSlot& slot,
	                const ErrorSlot& error ) const
	{

		aCall<void>( connected_, 
		             boost::bind( xmmsc_playlist_sort, conn_, 
		                          property.c_str() ),
		             slot, error );

	}

	void
	Playlist::sort( const std::string& property,
	                const std::list< VoidSlot >& slots,
	                const ErrorSlot& error ) const
	{

		aCall<void>( connected_, 
		             boost::bind( xmmsc_playlist_sort, conn_, 
		                          property.c_str() ),
		             slots, error );

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
	Playlist::broadcastChanged( const std::list< DictSlot >& slots,
	                            const ErrorSlot& error ) const
	{

		aCall<Dict>( connected_,
		             boost::bind( xmmsc_broadcast_playlist_changed, conn_ ),
					 slots, error );

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

	void
	Playlist::broadcastCurrentPos( const std::list< UintSlot >& slots,
	                               const ErrorSlot& error ) const
	{

		aCall<unsigned int>( connected_,
		                     boost::bind( xmmsc_broadcast_playlist_current_pos,
		                                  conn_ ),
		                     slots, error );

	}

	Playlist::Playlist( xmmsc_connection_t*& conn, bool& connected,
	                    MainloopInterface*& ml ) :
		conn_( conn ), connected_( connected ), ml_( ml )
	{
	}

}
