#ifndef XMMSCLIENTPP_PLAYBACK_H
#define XMMSCLIENTPP_PLAYBACK_H

#include <xmmsclient/xmmsclient.h>
#include <xmmsclient/xmmsclient++/mainloop.h>

namespace Xmms 
{

	class Client;
	class Playback 
	{

		public:

			typedef xmms_playback_status_t Status;

			static const Status STOPPED = XMMS_PLAYBACK_STATUS_STOP;
			static const Status PLAYING = XMMS_PLAYBACK_STATUS_PLAY;
			static const Status PAUSED  = XMMS_PLAYBACK_STATUS_PAUSE;

			// Destructor
			virtual ~Playback();

			// Commands

			// Control
			void tickle() const;
			void stop() const;
			void pause() const;
			void start() const;

			// Status
			// Compare returned value with 
			// Xmms::Playback::[STOPPED|PLAYING|PAUSED]
			Status getStatus() const;

		private:

			// Constructor, only to be called by Xmms::Client
			friend class Client;
			Playback( xmmsc_connection_t*& conn, bool& connected,
			          MainLoop*& ml );

			// Copy-constructor / operator=
			Playback( const Playback& src );
			Playback operator=( const Playback& src ) const;

			xmmsc_connection_t*& conn_;
			bool& connected_;
			MainLoop*& ml_;

	};

}

#endif // XMMSCLIENTPP_PLAYBACK_H
