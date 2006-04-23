#include <xmmsclient/xmmsclient.h>
#include <xmmsclient/xmmsclient++/client.h>
#include <xmmsclient/xmmsclient++/playback.h>
#include <xmmsclient/xmmsclient++/signal.h>
#include <xmmsclient/xmmsclient++/helpers.h>
#include <xmmsclient/xmmsclient++/mainloop.h>
#include <xmmsclient/xmmsclient++/typedefs.h>

#include <boost/bind.hpp>

#include <list>

namespace Xmms
{
	
	Playback::~Playback()
	{
	}

	void Playback::tickle() const
	{

		vCall( connected_, ml_,
		      boost::bind( xmmsc_playback_tickle, conn_ ) );

	}

	void Playback::stop() const
	{

		vCall( connected_, ml_,
		      boost::bind( xmmsc_playback_stop, conn_ ) );
		
	}

	void Playback::pause() const
	{

		vCall( connected_, ml_,
		      boost::bind( xmmsc_playback_pause, conn_ ) );

	}

	void Playback::start() const
	{

		vCall( connected_, ml_,
		       boost::bind( xmmsc_playback_start, conn_ ) );

	}

	Playback::Status Playback::getStatus() const
	{

		xmmsc_result_t* res = 
		    call( connected_, ml_,
		          boost::bind( xmmsc_playback_status, conn_ ) );

		unsigned int status = 0;
		xmmsc_result_get_uint( res, &status );
		xmmsc_result_unref( res );

		return static_cast< Playback::Status >(status);

	}

	void Playback::tickle( const VoidSlot& slot,
	                       const ErrorSlot& error ) const
	{
		aCall<void>( connected_, boost::bind( xmmsc_playback_tickle, conn_ ),
		             slot, error );
	}

	void Playback::tickle( const std::list< VoidSlot >& slots,
	                       const ErrorSlot& error ) const
	{
		aCall<void>( connected_, boost::bind( xmmsc_playback_tickle, conn_ ),
		             slots, error );
	}

	void Playback::stop( const VoidSlot& slot,
	                     const ErrorSlot& error ) const
	{
		aCall<void>( connected_, boost::bind( xmmsc_playback_stop, conn_ ),
		             slot, error );
	}

	void Playback::stop( const std::list< VoidSlot >& slots,
	                     const ErrorSlot& error ) const
	{
		aCall<void>( connected_, boost::bind( xmmsc_playback_stop, conn_ ),
		             slots, error );
	}

	void Playback::pause( const VoidSlot& slot,
	                      const ErrorSlot& error ) const
	{
		aCall<void>( connected_, boost::bind( xmmsc_playback_pause, conn_ ),
		             slot, error );
	}

	void Playback::pause( const std::list< VoidSlot >& slots,
	                      const ErrorSlot& error ) const
	{
		aCall<void>( connected_, boost::bind( xmmsc_playback_pause, conn_ ),
		             slots, error );
	}

	void Playback::start( const VoidSlot& slot,
	                      const ErrorSlot& error ) const
	{
		aCall<void>( connected_, boost::bind( xmmsc_playback_start, conn_ ),
		             slot, error );
	}

	void Playback::start( const std::list< VoidSlot >& slots,
	                      const ErrorSlot& error ) const
	{
		aCall<void>( connected_, boost::bind( xmmsc_playback_start, conn_ ),
		             slots, error );
	}

	void Playback::getStatus( const StatusSlot& slot,
	                          const ErrorSlot& error ) const
	{
		aCall<Status>( connected_, boost::bind( xmmsc_playback_status, conn_ ),
		               slot, error );
	}

	void Playback::getStatus( const std::list< StatusSlot >& slots,
	                          const ErrorSlot& error ) const
	{
		aCall<Status>( connected_, boost::bind( xmmsc_playback_status, conn_ ),
		               slots, error );
	}

	Playback::Playback( xmmsc_connection_t*& conn, bool& connected,
	                    MainLoop*& ml ) :
		conn_( conn ), connected_( connected ), ml_( ml )
	{
	}

}
