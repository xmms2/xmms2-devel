#include <xmmsclient/xmmsclient.h>
#include <xmmsclient/xmmsclient++/client.h>
#include <xmmsclient/xmmsclient++/playback.h>
#include <xmmsclient/xmmsclient++/signal.h>
#include <xmmsclient/xmmsclient++/helpers.h>
#include <xmmsclient/xmmsclient++/mainloop.h>
#include <xmmsclient/xmmsclient++/typedefs.h>
#include <xmmsclient/xmmsclient++/dict.h>

#include <boost/bind.hpp>

#include <list>
#include <string>

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

	void Playback::seekMs(unsigned int milliseconds) const
	{
		vCall( connected_, ml_,
		       boost::bind( xmmsc_playback_seek_ms, conn_, milliseconds ) );
	}

	void Playback::seekMsRel(int milliseconds) const
	{
		vCall( connected_, ml_,
		       boost::bind( xmmsc_playback_seek_ms_rel, conn_, milliseconds ) );
	}

	void Playback::seekSamples(unsigned int samples) const
	{
		vCall( connected_, ml_,
		       boost::bind( xmmsc_playback_seek_samples, conn_, samples ) );
	}

	void Playback::seekSamplesRel(int samples) const
	{
		vCall( connected_, ml_,
		       boost::bind( xmmsc_playback_seek_samples_rel, conn_, samples ) );
	}

	unsigned int Playback::currentID() const
	{
		xmmsc_result_t* res = 
		    call( connected_, ml_,
		          boost::bind( xmmsc_playback_current_id, conn_ ) );
		unsigned int id = 0;
		xmmsc_result_get_uint( res, &id );
		xmmsc_result_unref( res );

		return id;
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

	unsigned int Playback::getPlaytime() const
	{
		xmmsc_result_t* res = 
		    call( connected_, ml_,
		          boost::bind( xmmsc_playback_playtime, conn_ ) );
		unsigned int playtime = 0;
		xmmsc_result_get_uint( res, &playtime );
		xmmsc_result_unref( res );

		return playtime;
	}

	void Playback::volumeSet(const std::string& channel,
	                         unsigned int volume) const
	{
		vCall( connected_, ml_,
		       boost::bind( xmmsc_playback_volume_set, conn_,
		                    channel.c_str(), volume ) );
	}

	Dict Playback::volumeGet() const
	{
		xmmsc_result_t* res = 
		    call( connected_, ml_,
		          boost::bind( xmmsc_playback_volume_get, conn_ ) );
		Dict volume( res );
		xmmsc_result_unref( res );

		return volume;
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

	void Playback::seekMs( unsigned int milliseconds,
	                       const VoidSlot& slot,
	                       const ErrorSlot& error ) const
	{
		using boost::bind;
		aCall<void>( connected_,
		             bind( xmmsc_playback_seek_ms, conn_, milliseconds ),
		             slot, error );
	}

	void Playback::seekMs( unsigned int milliseconds,
	                       const std::list< VoidSlot >& slots,
	                       const ErrorSlot& error ) const
	{
		using boost::bind;
		aCall<void>( connected_,
		             bind( xmmsc_playback_seek_ms, conn_, milliseconds ),
		             slots, error );
	}

	void Playback::seekMsRel( int milliseconds,
	                          const VoidSlot& slot,
	                          const ErrorSlot& error ) const
	{
		using boost::bind;
		aCall<void>( connected_,
		             bind( xmmsc_playback_seek_ms_rel, conn_, milliseconds ),
		             slot, error );
	}

	void Playback::seekMsRel( int milliseconds,
	                          const std::list< VoidSlot >& slots,
	                          const ErrorSlot& error ) const
	{
		using boost::bind;
		aCall<void>( connected_,
		             bind( xmmsc_playback_seek_ms_rel, conn_, milliseconds ),
		             slots, error );
	}

	void Playback::seekSamples( unsigned int samples,
	                            const VoidSlot& slot,
	                            const ErrorSlot& error ) const
	{
		using boost::bind;
		aCall<void>( connected_,
		             bind( xmmsc_playback_seek_samples, conn_, samples ),
		             slot, error );
	}

	void Playback::seekSamples( unsigned int samples,
	                            const std::list< VoidSlot >& slots,
	                            const ErrorSlot& error ) const
	{
		using boost::bind;
		aCall<void>( connected_,
		             bind( xmmsc_playback_seek_samples, conn_, samples ),
		             slots, error );
	}

	void Playback::seekSamplesRel( int samples,
	                               const VoidSlot& slot,
	                               const ErrorSlot& error ) const
	{
		using boost::bind;
		aCall<void>( connected_,
		             bind( xmmsc_playback_seek_samples_rel, conn_, samples ),
		             slot, error );
	}

	void Playback::seekSamplesRel( int samples,
	                               const std::list< VoidSlot >& slots,
	                               const ErrorSlot& error ) const
	{
		using boost::bind;
		aCall<void>( connected_,
		             bind( xmmsc_playback_seek_samples_rel, conn_, samples ),
		             slots, error );
	}

	void Playback::currentID( const UintSlot& slot,
	                          const ErrorSlot& error ) const
	{
		aCall<unsigned int>( connected_, 
		                     boost::bind( xmmsc_playback_current_id, conn_ ),
		                     slot, error );
	}

	void Playback::currentID( const std::list< UintSlot >& slots,
	                          const ErrorSlot& error ) const
	{
		aCall<unsigned int>( connected_, 
		                     boost::bind( xmmsc_playback_current_id, conn_ ),
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

	void Playback::getPlaytime( const UintSlot& slot,
	                            const ErrorSlot& error ) const
	{
		aCall<unsigned int>( connected_,
		                     boost::bind( xmmsc_playback_playtime, conn_ ),
		                     slot, error );
	}

	void Playback::getPlaytime( const std::list< UintSlot >& slots,
	                            const ErrorSlot& error ) const
	{
		aCall<unsigned int>( connected_,
		                     boost::bind( xmmsc_playback_playtime, conn_ ),
		                     slots, error );
	}

	void Playback::volumeSet( const std::string& channel, unsigned int volume,
	                          const VoidSlot& slot,
	                          const ErrorSlot& error ) const
	{
		aCall<void>( connected_,
		             boost::bind( xmmsc_playback_volume_set, conn_,
		                          channel.c_str(), volume ),
		             slot, error );
	}

	void Playback::volumeSet( const std::string& channel, unsigned int volume,
	                          const std::list< VoidSlot >& slots,
	                          const ErrorSlot& error ) const
	{
		aCall<void>( connected_,
		             boost::bind( xmmsc_playback_volume_set, conn_,
		                          channel.c_str(), volume ),
		             slots, error );
	}

	void Playback::volumeGet( const DictSlot& slot,
	                          const ErrorSlot& error ) const
	{
		aCall<Dict>( connected_,
		             boost::bind( xmmsc_playback_volume_get, conn_ ),
		             slot, error );
	}

	void Playback::volumeGet( const std::list< DictSlot >& slots,
	                          const ErrorSlot& error ) const
	{
		aCall<Dict>( connected_,
		             boost::bind( xmmsc_playback_volume_get, conn_ ),
		             slots, error );
	}

	void
	Playback::broadcastCurrentID( const UintSlot& slot,
	                              const ErrorSlot& error ) const
	{
		aCall<unsigned int>( connected_,
		                     boost::bind( xmmsc_broadcast_playback_current_id,
		                                  conn_ ),
		                     slot, error );
	}

	void
	Playback::broadcastCurrentID( const std::list< UintSlot >& slots,
	                              const ErrorSlot& error ) const
	{
		aCall<unsigned int>( connected_,
		                     boost::bind( xmmsc_broadcast_playback_current_id,
		                                  conn_ ),
		                     slots, error );
	}

	void
	Playback::broadcastStatus( const StatusSlot& slot,
	                           const ErrorSlot& error ) const
	{
		aCall<Status>( connected_,
		               boost::bind( xmmsc_broadcast_playback_status,
		                            conn_ ),
		               slot, error );
	}

	void
	Playback::broadcastStatus( const std::list< StatusSlot >& slots,
	                           const ErrorSlot& error ) const
	{
		aCall<Status>( connected_,
		               boost::bind( xmmsc_broadcast_playback_status,
		                            conn_ ),
		               slots, error );
	}

	void
	Playback::broadcastVolumeChanged( const DictSlot& slot,
	                                  const ErrorSlot& error ) const
	{
		aCall<Dict>( connected_,
		             boost::bind( xmmsc_broadcast_playback_volume_changed,
		                          conn_ ),
		             slot, error );
	}

	void
	Playback::broadcastVolumeChanged( const std::list< DictSlot >& slots,
	                                  const ErrorSlot& error ) const
	{
		aCall<Dict>( connected_,
		             boost::bind( xmmsc_broadcast_playback_volume_changed,
		                          conn_ ),
		             slots, error );
	}

	void
	Playback::signalPlaytime( const UintSlot& slot,
	                          const ErrorSlot& error ) const
	{
		aCall<unsigned int>( connected_,
		                     boost::bind( xmmsc_signal_playback_playtime,
		                                  conn_ ),
		                     slot, error );
	}

	void
	Playback::signalPlaytime( const std::list< UintSlot >& slots,
	                          const ErrorSlot& error ) const
	{
		aCall<unsigned int>( connected_,
		                     boost::bind( xmmsc_signal_playback_playtime,
		                                  conn_ ),
		                     slots, error );
	}

	Playback::Playback( xmmsc_connection_t*& conn, bool& connected,
	                    MainloopInterface*& ml ) :
		conn_( conn ), connected_( connected ), ml_( ml )
	{
	}

}
