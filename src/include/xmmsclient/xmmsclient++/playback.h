#ifndef XMMSCLIENTPP_PLAYBACK_H
#define XMMSCLIENTPP_PLAYBACK_H

#include <xmmsclient/xmmsclient.h>
#include <xmmsclient/xmmsclient++/mainloop.h>
#include <xmmsclient/xmmsclient++/signal.h>
#include <xmmsclient/xmmsclient++/helpers.h>

#include <list>

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
 
			void tickle( const Signal<void>::signal_t::slot_type& slot,
			             const error_sig::slot_type& error = &Xmms::dummy_error
			           ) const;

			void tickle( const std::list<
			                   Signal<void>::signal_t::slot_type> slots,
			             const error_sig::slot_type& error = &Xmms::dummy_error
			           ) const;

			void stop( const Signal<void>::signal_t::slot_type& slot,
			           const error_sig::slot_type& error = &Xmms::dummy_error
			         ) const;

			void stop( const std::list<
			                 Signal<void>::signal_t::slot_type> slots,
			           const error_sig::slot_type& error = &Xmms::dummy_error
			         ) const;

			void pause( const Signal<void>::signal_t::slot_type& slot,
			            const error_sig::slot_type& error = &Xmms::dummy_error
			          ) const;

			void pause( const std::list<
			                  Signal<void>::signal_t::slot_type> slots,
			            const error_sig::slot_type& error = &Xmms::dummy_error
			          ) const;

			void start( const Signal<void>::signal_t::slot_type& slot,
			            const error_sig::slot_type& error = &Xmms::dummy_error
			          ) const;

			void start( const std::list<
			                  Signal<void>::signal_t::slot_type> slots,
			            const error_sig::slot_type& error = &Xmms::dummy_error
			          ) const;
 
			void getStatus( const Signal<Status>::signal_t::slot_type& slot,
			                const error_sig::slot_type& error
							= &Xmms::dummy_error
			              ) const;

			void getStatus( const std::list<
			                      Signal<Status>::signal_t::slot_type> slots,
			                const error_sig::slot_type& error
							= &Xmms::dummy_error
			              ) const;

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
