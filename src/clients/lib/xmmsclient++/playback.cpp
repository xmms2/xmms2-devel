/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2007 XMMS2 Team
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
#include <xmmsclient/xmmsclient++/client.h>
#include <xmmsclient/xmmsclient++/playback.h>
#include <xmmsclient/xmmsclient++/signal.h>
#include <xmmsclient/xmmsclient++/helpers.h>
#include <xmmsclient/xmmsclient++/mainloop.h>
#include <xmmsclient/xmmsclient++/typedefs.h>
#include <xmmsclient/xmmsclient++/dict.h>

#include <boost/bind.hpp>

#include <iostream>
#include <string>

namespace Xmms
{
	
	Playback::~Playback()
	{
	}

	VoidResult Playback::tickle() const
	{
		xmmsc_result_t* res =
			call( connected_, boost::bind( xmmsc_playback_tickle, conn_ ) );
		return VoidResult( res, ml_ );
	}

	VoidResult Playback::stop() const
	{
		xmmsc_result_t* res =
			call( connected_, boost::bind( xmmsc_playback_stop, conn_ ) );
		return VoidResult( res, ml_ );
	}

	VoidResult Playback::pause() const
	{
		xmmsc_result_t* res =
			call( connected_, boost::bind( xmmsc_playback_pause, conn_ ) );
		return VoidResult( res, ml_ );
	}

	VoidResult Playback::start() const
	{
		xmmsc_result_t* res =
			call( connected_, boost::bind( xmmsc_playback_start, conn_ ) );
		return VoidResult( res, ml_ );
	}

	VoidResult Playback::seekMs(unsigned int milliseconds) const
	{
		xmmsc_result_t* res =
			call( connected_,
			      boost::bind( xmmsc_playback_seek_ms, conn_, milliseconds ) );
		return VoidResult( res, ml_ );
	}

	VoidResult Playback::seekMsRel(int milliseconds) const
	{
		xmmsc_result_t* res =
			call( connected_,
			      boost::bind( xmmsc_playback_seek_ms_rel,
			                   conn_, milliseconds ) );
		return VoidResult( res, ml_ );
	}

	VoidResult Playback::seekSamples(unsigned int samples) const
	{
		xmmsc_result_t* res =
			call( connected_,
			      boost::bind( xmmsc_playback_seek_samples, conn_, samples ) );
		return VoidResult( res, ml_ );
	}

	VoidResult Playback::seekSamplesRel(int samples) const
	{
		xmmsc_result_t* res =
			call( connected_,
			      boost::bind( xmmsc_playback_seek_samples_rel,
			                   conn_, samples ) );
		return VoidResult( res, ml_ );
	}

	UintResult Playback::currentID() const
	{
		xmmsc_result_t* res = 
		    call( connected_,
		          boost::bind( xmmsc_playback_current_id, conn_ ) );
		return UintResult( res, ml_ );
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

	UintResult Playback::getPlaytime() const
	{
		xmmsc_result_t* res = 
		    call( connected_, boost::bind( xmmsc_playback_playtime, conn_ ) );
		return UintResult( res, ml_ );
	}

	VoidResult Playback::volumeSet(const std::string& channel,
	                               unsigned int volume) const
	{
		xmmsc_result_t* res =
			call( connected_,
			      boost::bind( xmmsc_playback_volume_set, conn_,
			                   channel.c_str(), volume ) );
		return VoidResult( res, ml_ );
	}

	DictResult Playback::volumeGet() const
	{
		xmmsc_result_t* res = 
		    call( connected_,
		          boost::bind( xmmsc_playback_volume_get, conn_ ) );
		return DictResult( res, ml_ );
	}

#if 0
	void Playback::tickle( const VoidSlot& slot,
	                       const ErrorSlot& error ) const
	{
		aCall<void>( connected_, boost::bind( xmmsc_playback_tickle, conn_ ),
		             slot, error );
	}

	void Playback::stop( const VoidSlot& slot,
	                     const ErrorSlot& error ) const
	{
		aCall<void>( connected_, boost::bind( xmmsc_playback_stop, conn_ ),
		             slot, error );
	}

	void Playback::pause( const VoidSlot& slot,
	                      const ErrorSlot& error ) const
	{
		aCall<void>( connected_, boost::bind( xmmsc_playback_pause, conn_ ),
		             slot, error );
	}

	void Playback::start( const VoidSlot& slot,
	                      const ErrorSlot& error ) const
	{
		aCall<void>( connected_, boost::bind( xmmsc_playback_start, conn_ ),
		             slot, error );
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

	void Playback::seekMsRel( int milliseconds,
	                          const VoidSlot& slot,
	                          const ErrorSlot& error ) const
	{
		using boost::bind;
		aCall<void>( connected_,
		             bind( xmmsc_playback_seek_ms_rel, conn_, milliseconds ),
		             slot, error );
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

	void Playback::seekSamplesRel( int samples,
	                               const VoidSlot& slot,
	                               const ErrorSlot& error ) const
	{
		using boost::bind;
		aCall<void>( connected_,
		             bind( xmmsc_playback_seek_samples_rel, conn_, samples ),
		             slot, error );
	}

	void Playback::currentID( const UintSlot& slot,
	                          const ErrorSlot& error ) const
	{
		aCall<unsigned int>( connected_, 
		                     boost::bind( xmmsc_playback_current_id, conn_ ),
		                     slot, error );
	}

	void Playback::getStatus( const StatusSlot& slot,
	                          const ErrorSlot& error ) const
	{
		aCall<Status>( connected_, boost::bind( xmmsc_playback_status, conn_ ),
		               slot, error );
	}

	void Playback::getPlaytime( const UintSlot& slot,
	                            const ErrorSlot& error ) const
	{
		aCall<unsigned int>( connected_,
		                     boost::bind( xmmsc_playback_playtime, conn_ ),
		                     slot, error );
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

	void Playback::volumeGet( const DictSlot& slot,
	                          const ErrorSlot& error ) const
	{
		aCall<Dict>( connected_,
		             boost::bind( xmmsc_playback_volume_get, conn_ ),
		             slot, error );
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
	Playback::broadcastStatus( const StatusSlot& slot,
	                           const ErrorSlot& error ) const
	{
		aCall<Status>( connected_,
		               boost::bind( xmmsc_broadcast_playback_status,
		                            conn_ ),
		               slot, error );
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
	Playback::signalPlaytime( const UintSlot& slot,
	                          const ErrorSlot& error ) const
	{
		aCall<unsigned int>( connected_,
		                     boost::bind( xmmsc_signal_playback_playtime,
		                                  conn_ ),
		                     slot, error );
	}
#endif

	Playback::Playback( xmmsc_connection_t*& conn, bool& connected,
	                    MainloopInterface*& ml ) :
		conn_( conn ), connected_( connected ), ml_( ml )
	{
	}

}
