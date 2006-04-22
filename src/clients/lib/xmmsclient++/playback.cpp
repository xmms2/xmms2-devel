#include <xmmsclient/xmmsclient.h>
#include <xmmsclient/xmmsclient++/client.h>
#include <xmmsclient/xmmsclient++/playback.h>
#include <xmmsclient/xmmsclient++/signal.h>
#include <xmmsclient/xmmsclient++/helpers.h>
#include <xmmsclient/xmmsclient++/mainloop.h>

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

	void Playback::tickle( const Signal<void>::signal_t::slot_type& slot,
	                       const error_sig::slot_type& error ) const
	{
		aCall<void>( connected_, boost::bind( xmmsc_playback_tickle, conn_ ),
		             slot, error );
	}

	void Playback::tickle( const std::list<
	                             Signal<void>::signal_t::slot_type> slots,
	                       const error_sig::slot_type& error ) const
	{
		aCall<void>( connected_, boost::bind( xmmsc_playback_tickle, conn_ ),
		             slots, error );
	}

	void Playback::stop( const Signal<void>::signal_t::slot_type& slot,
	                     const error_sig::slot_type& error ) const
	{
		aCall<void>( connected_, boost::bind( xmmsc_playback_stop, conn_ ),
		             slot, error );
	}

	void Playback::stop( const std::list<
	                           Signal<void>::signal_t::slot_type> slots,
	                     const error_sig::slot_type& error ) const
	{
		aCall<void>( connected_, boost::bind( xmmsc_playback_stop, conn_ ),
		             slots, error );
	}

	void Playback::pause( const Signal<void>::signal_t::slot_type& slot,
	                      const error_sig::slot_type& error ) const
	{
		aCall<void>( connected_, boost::bind( xmmsc_playback_pause, conn_ ),
		             slot, error );
	}

	void Playback::pause( const std::list<
	                            Signal<void>::signal_t::slot_type> slots,
	                      const error_sig::slot_type& error ) const
	{
		aCall<void>( connected_, boost::bind( xmmsc_playback_pause, conn_ ),
		             slots, error );
	}

	void Playback::start( const Signal<void>::signal_t::slot_type& slot,
	                      const error_sig::slot_type& error ) const
	{
		aCall<void>( connected_, boost::bind( xmmsc_playback_start, conn_ ),
		             slot, error );
	}

	void Playback::start( const std::list<
	                            Signal<void>::signal_t::slot_type> slots,
	                      const error_sig::slot_type& error ) const
	{
		aCall<void>( connected_, boost::bind( xmmsc_playback_start, conn_ ),
		             slots, error );
	}

	void Playback::getStatus( const Signal<Status>::signal_t::slot_type& slot,
	                          const error_sig::slot_type& error ) const
	{
		aCall<Status>( connected_, boost::bind( xmmsc_playback_status, conn_ ),
		               slot, error );
	}

	void Playback::getStatus( const std::list<
	                                Signal<Status>::signal_t::slot_type> slots,
	                          const error_sig::slot_type& error ) const
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
