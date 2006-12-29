/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2006 XMMS2 Team
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

#ifndef XMMSCLIENTPP_PLAYBACK_H
#define XMMSCLIENTPP_PLAYBACK_H

#include <xmmsclient/xmmsclient.h>
#include <xmmsclient/xmmsclient++/mainloop.h>
#include <xmmsclient/xmmsclient++/signal.h>
#include <xmmsclient/xmmsclient++/helpers.h>
#include <xmmsclient/xmmsclient++/typedefs.h>
#include <xmmsclient/xmmsclient++/dict.h>

#include <string>

namespace Xmms 
{

	class Client;
	/** @class Playback playback.h "xmmsclient/xmmsclient++/playback.h"
	 *  @brief This class controls the playback.
	 */
	class Playback 
	{

		public:

			typedef xmms_playback_status_t Status;

			static const Status STOPPED = XMMS_PLAYBACK_STATUS_STOP;
			static const Status PLAYING = XMMS_PLAYBACK_STATUS_PLAY;
			static const Status PAUSED  = XMMS_PLAYBACK_STATUS_PAUSE;

			/** Destructor.
			 */
			virtual ~Playback();

			/** Stop decoding of current song.
			 *
			 *  This will start decoding of the song set with
			 *  Playlist::setNext, or the current song again if no
			 *  Playlist::setNext was executed.
			 *
			 *  @throw connection_error If the client isn't connected.
			 *  @throw mainloop_running_error If a mainloop is running -
			 *  sync functions can't be called when mainloop is running. This
			 *  is only thrown if the programmer is careless or doesn't know
			 *  what he/she's doing. (logic_error)
			 *  @throw result_error If the operation failed.
			 */
			void tickle() const;

			/** Stops the current playback.
			 * 
			 *  This will make the server idle.
			 *
			 *  @throw connection_error If the client isn't connected.
			 *  @throw mainloop_running_error If a mainloop is running -
			 *  sync functions can't be called when mainloop is running. This
			 *  is only thrown if the programmer is careless or doesn't know
			 *  what he/she's doing. (logic_error)
			 *  @throw result_error If the operation failed.
			 */
			void stop() const;

			/** Pause the current playback,
			 *  will tell the output to not read nor write.
			 *
			 *  @throw connection_error If the client isn't connected.
			 *  @throw mainloop_running_error If a mainloop is running -
			 *  sync functions can't be called when mainloop is running. This
			 *  is only thrown if the programmer is careless or doesn't know
			 *  what he/she's doing. (logic_error)
			 *  @throw result_error If the operation failed.
			 */
			void pause() const;

			/** Starts playback if server is idle.
			 *
			 *  @throw connection_error If the client isn't connected.
			 *  @throw mainloop_running_error If a mainloop is running -
			 *  sync functions can't be called when mainloop is running. This
			 *  is only thrown if the programmer is careless or doesn't know
			 *  what he/she's doing. (logic_error)
			 *  @throw result_error If the operation failed.
			 */
			void start() const;

			/** Seek to a absolute time in the current playback.
			 *
			 *  @param milliseconds The total number of ms where
			 *                      playback should continue.
			 *
			 *  @throw connection_error If the client isn't connected.
			 *  @throw mainloop_running_error If a mainloop is running -
			 *  sync functions can't be called when mainloop is running. This
			 *  is only thrown if the programmer is careless or doesn't know
			 *  what he/she's doing. (logic_error)
			 *  @throw result_error If the operation failed.
			 */
			void seekMs(unsigned int milliseconds) const;

			/** Seek to a time relative to the current position 
			 *  in the current playback.
			 *
			 *  @param milliseconds The offset in ms from the current
			 *                      position to where playback should continue.
			 *
			 *  @throw connection_error If the client isn't connected.
			 *  @throw mainloop_running_error If a mainloop is running -
			 *  sync functions can't be called when mainloop is running. This
			 *  is only thrown if the programmer is careless or doesn't know
			 *  what he/she's doing. (logic_error)
			 *  @throw result_error If the operation failed.
			 */
			void seekMsRel(int milliseconds) const;

			/** Seek to a absolute number of samples in the current playback.
			 *
			 *  @param samples The total number of samples where
			 *                 playback should continue.
			 *
			 *  @throw connection_error If the client isn't connected.
			 *  @throw mainloop_running_error If a mainloop is running -
			 *  sync functions can't be called when mainloop is running. This
			 *  is only thrown if the programmer is careless or doesn't know
			 *  what he/she's doing. (logic_error)
			 *  @throw result_error If the operation failed.
			 */
			void seekSamples(unsigned int samples) const;

			/** Seek to a number of samples relative to the current
			 *  position in the current playback.
			 *
			 *  @param samples The offset in number of samples from the current
			 *                 position to where playback should continue.
			 *
			 *  @throw connection_error If the client isn't connected.
			 *  @throw mainloop_running_error If a mainloop is running -
			 *  sync functions can't be called when mainloop is running. This
			 *  is only thrown if the programmer is careless or doesn't know
			 *  what he/she's doing. (logic_error)
			 *  @throw result_error If the operation failed.
			 */
			void seekSamplesRel(int samples) const;

			/** Make server emit the current id.
			 *
			 *  @throw connection_error If the client isn't connected.
			 *  @throw mainloop_running_error If a mainloop is running -
			 *  sync functions can't be called when mainloop is running. This
			 *  is only thrown if the programmer is careless or doesn't know
			 *  what he/she's doing. (logic_error)
			 *  @throw result_error If the operation failed.
			 *
			 *  @return The currently playing ID.
			 */
			unsigned int currentID() const;

			/** Make server emit the playback status.
			 *
			 *  @throw connection_error If the client isn't connected.
			 *  @throw mainloop_running_error If a mainloop is running -
			 *  sync functions can't be called when mainloop is running. This
			 *  is only thrown if the programmer is careless or doesn't know
			 *  what he/she's doing. (logic_error)
			 *  @throw result_error If the operation failed.
			 *
			 *  @return Playback status, compare with
			 *          Playback::[STOPPED|PLAYING|PAUSED]
			 */
			Status getStatus() const;

			/** Make server emit the current playtime.
			 *
			 *  @throw connection_error If the client isn't connected.
			 *  @throw mainloop_running_error If a mainloop is running -
			 *  sync functions can't be called when mainloop is running. This
			 *  is only thrown if the programmer is careless or doesn't know
			 *  what he/she's doing. (logic_error)
			 *  @throw result_error If the operation failed.
			 *
			 *  @return The playtime in milliseconds.
			 */
			unsigned int getPlaytime() const;

			/** Set the volume of a channel.
			 *
			 *  @param channel Name of the channel.
			 *  @param volume Volume within range [0-100].
			 *
			 *  @throw connection_error If the client isn't connected.
			 *  @throw mainloop_running_error If a mainloop is running -
			 *  sync functions can't be called when mainloop is running. This
			 *  is only thrown if the programmer is careless or doesn't know
			 *  what he/she's doing. (logic_error)
			 *  @throw result_error If the operation failed.
			 */
			void volumeSet(const std::string& channel,
			               unsigned int volume) const;

			/** Get a channel<->volume list from the server.
			 *
			 *  @throw connection_error If the client isn't connected.
			 *  @throw mainloop_running_error If a mainloop is running -
			 *  sync functions can't be called when mainloop is running. This
			 *  is only thrown if the programmer is careless or doesn't know
			 *  what he/she's doing. (logic_error)
			 *  @throw result_error If the operation failed.
			 *
			 *  @return A Dict containing channel<->volume pairs.
			 */
			Dict volumeGet() const;

			/** Stop decoding of current song.
			 *
			 *  This will start decoding of the song set with
			 *  Playlist::setNext, or the current song again if no
			 *  Playlist::setNext was executed.
			 *
			 *  @param slot Function pointer to a function returning a bool.
			 *  @param error Function pointer to an error callback
			 *               function. (<b>optional</b>)
			 *
			 *  @throw connection_error If the client isn't connected.
			 */
			void tickle( const VoidSlot& slot,
			             const ErrorSlot& error = &Xmms::dummy_error ) const;

			/** Stops the current playback.
			 * 
			 *  This will make the server idle.
			 *
			 *  @param slot Function pointer to a function returning a bool.
			 *  @param error Function pointer to an error callback
			 *               function. (<b>optional</b>)
			 *
			 *  @throw connection_error If the client isn't connected.
			 */
			void stop( const VoidSlot& slot,
			           const ErrorSlot& error = &Xmms::dummy_error ) const;

			/** Pause the current playback,
			 *  will tell the output to not read nor write.
			 *
			 *  @param slot Function pointer to a function returning a bool.
			 *  @param error Function pointer to an error callback
			 *               function. (<b>optional</b>)
			 *
			 *  @throw connection_error If the client isn't connected.
			 */
			void pause( const VoidSlot& slot,
			            const ErrorSlot& error = &Xmms::dummy_error ) const;

			/** Starts playback if server is idle.
			 *
			 *  @param slot Function pointer to a function returning a bool.
			 *  @param error Function pointer to an error callback
			 *               function. (<b>optional</b>)
			 *
			 *  @throw connection_error If the client isn't connected.
			 */
			void start( const VoidSlot& slot,
			            const ErrorSlot& error = &Xmms::dummy_error ) const;

			/** Seek to a absolute time in the current playback.
			 *
			 *  @param milliseconds The total number of ms where
			 *                      playback should continue.
			 *  @param slot Function pointer to a function returning a bool.
			 *  @param error Function pointer to an error callback
			 *               function. (<b>optional</b>)
			 *
			 *  @throw connection_error If the client isn't connected.
			 */
			void seekMs( unsigned int milliseconds,
			             const VoidSlot& slot,
			             const ErrorSlot& error = &Xmms::dummy_error ) const;

			/** Seek to a time relative to the current position 
			 *  in the current playback.
			 *
			 *  @param milliseconds The offset in ms from the current
			 *                      position to where playback should continue.
			 *  @param slot Function pointer to a function returning a bool.
			 *  @param error Function pointer to an error callback
			 *               function. (<b>optional</b>)
			 *
			 *  @throw connection_error If the client isn't connected.
			 */
			void seekMsRel( int milliseconds,
			                const VoidSlot& slot,
			                const ErrorSlot& error = &Xmms::dummy_error ) const;

			/** Seek to a absolute number of samples in the current playback.
			 *
			 *  @param samples The total number of samples where
			 *                 playback should continue.
			 *  @param slot Function pointer to a function returning a bool.
			 *  @param error Function pointer to an error callback
			 *               function. (<b>optional</b>)
			 *
			 *  @throw connection_error If the client isn't connected.
			 */
			void seekSamples( unsigned int samples,
			                  const VoidSlot& slot,
			                  const ErrorSlot& error = &Xmms::dummy_error
			                ) const;

			/** Seek to a number of samples relative to the current
			 *  position in the current playback.
			 *
			 *  @param samples The offset in number of samples from the current
			 *                 position to where playback should continue.
			 *  @param slot Function pointer to a function returning a bool.
			 *  @param error Function pointer to an error callback
			 *               function. (<b>optional</b>)
			 *
			 *  @throw connection_error If the client isn't connected.
			 */
			void seekSamplesRel( int samples,
			                     const VoidSlot& slot,
			                     const ErrorSlot& error = &Xmms::dummy_error
			                     ) const;

			/** Make server emit the current id.
			 *
			 *  @param slot Function pointer to a function taking
			 *              const unsigned int& and returning a bool.
			 *  @param error Function pointer to an error callback
			 *               function. (<b>optional</b>)
			 *
			 *  @throw connection_error If the client isn't connected.
			 */
			void currentID( const UintSlot& slot,
			                const ErrorSlot& error = &Xmms::dummy_error ) const;
 
			/** Make server emit the playback status.
			 *
			 *  @param slot Function pointer to a function taking
			 *              const Playback::Status& and returning a bool.
			 *  @param error Function pointer to an error callback
			 *               function. (<b>optional</b>)
			 *
			 *  @throw connection_error If the client isn't connected.
			 */
			void getStatus( const StatusSlot& slot,
			                const ErrorSlot& error = &Xmms::dummy_error ) const;

			/** Make server emit the current playtime.
			 *
			 *  @param slot Function pointer to a function taking
			 *              const unsigned int& and returning a bool.
			 *  @param error Function pointer to an error callback
			 *               function. (<b>optional</b>)
			 *
			 *  @throw connection_error If the client isn't connected.
			 */
			void getPlaytime( const UintSlot& slot,
			                  const ErrorSlot& error = &Xmms::dummy_error
			                ) const;

			/** Set the volume of a channel.
			 *
			 *  @param channel Name of the channel.
			 *  @param volume Volume within range [0-100].
			 *  @param slot Function pointer to a function returning a bool.
			 *  @param error Function pointer to an error callback
			 *               function. (<b>optional</b>)
			 *
			 *  @throw connection_error If the client isn't connected.
			 */
			void volumeSet( const std::string& channel, unsigned int volume,
			                const VoidSlot& slot,
			                const ErrorSlot& error = &Xmms::dummy_error
			              ) const;

			/** Get a channel<->volume list from the server.
			 *
			 *  @param slot Function pointer to a function taking
			 *              const Dict& and returning a bool.
			 *  @param error Function pointer to an error callback
			 *               function. (<b>optional</b>)
			 *
			 *  @throw connection_error If the client isn't connected.
			 */
			void volumeGet( const DictSlot& slot,
			                const ErrorSlot& error = &Xmms::dummy_error
			              ) const;

			/** Request the current id broadcast.
			 *  
			 *  This will be called when the current playing id is changed.
			 *  New song for example.
			 *
			 *  @param slot Function pointer to a function taking
			 *              const unsigned int& and returning a bool.
			 *  @param error Function pointer to an error callback
			 *               function. (<b>optional</b>)
			 *
			 *  @throw connection_error If the client isn't connected.
			 */
			void
			broadcastCurrentID( const UintSlot& slot,
			                    const ErrorSlot& error = &Xmms::dummy_error
			                  ) const;

			/** Request the status broadcast.
			 *
			 *  This will be called when the playback status changes.
			 *
			 *  @param slot Function pointer to a function taking
			 *              const Playback::Status& and returning a bool.
			 *  @param error Function pointer to an error callback
			 *               function. (<b>optional</b>)
			 *               
			 *  @throw connection_error If the client isn't connected.
			 */
			void
			broadcastStatus( const StatusSlot& slot,
			                 const ErrorSlot& error = &Xmms::dummy_error
			               ) const;

			/** Request volume changed broadcast.
			 *
			 *  This will be called when the volume is changed.
			 *
			 *  @param slot Function pointer to a function taking
			 *              const Dict& and returning a bool.
			 *  @param error Function pointer to an error callback
			 *               function. (<b>optional</b>)
			 *
			 *  @throw connection_error If the client isn't connected.
			 */
			void
			broadcastVolumeChanged( const DictSlot& slot,
			                        const ErrorSlot& error = &Xmms::dummy_error
			                      ) const;

			/** Request the playtime signal.
			 *
			 *  This will update the time we have played the current entry.
			 *
			 *  @param slot Function pointer to a function taking
			 *              const unsigned int& and returning a bool.
			 *  @param error Function pointer to an error callback
			 *               function. (<b>optional</b>)
			 *
			 *  @throw connection_error If the client isn't connected.
			 */
			void
			signalPlaytime( const UintSlot& slot,
			                const ErrorSlot& error = &Xmms::dummy_error ) const;

		/** @cond */
		private:

			// Constructor, only to be called by Xmms::Client
			friend class Client;
			Playback( xmmsc_connection_t*& conn, bool& connected,
			          MainloopInterface*& ml );

			// Copy-constructor / operator=
			Playback( const Playback& src );
			Playback operator=( const Playback& src ) const;

			xmmsc_connection_t*& conn_;
			bool& connected_;
			MainloopInterface*& ml_;
		/** @endcond */

	};

}

#endif // XMMSCLIENTPP_PLAYBACK_H
