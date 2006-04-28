#ifndef XMMSCLIENTPP_CLIENT_H
#define XMMSCLIENTPP_CLIENT_H

#include <xmmsclient/xmmsclient.h>

#include <xmmsclient/xmmsclient++/typedefs.h>
#include <xmmsclient/xmmsclient++/playback.h>
#include <xmmsclient/xmmsclient++/playlist.h>
#include <xmmsclient/xmmsclient++/medialib.h>
#include <xmmsclient/xmmsclient++/config.h>
#include <xmmsclient/xmmsclient++/stats.h>
#include <xmmsclient/xmmsclient++/mainloop.h>
#include <xmmsclient/xmmsclient++/listener.h>
#include <xmmsclient/xmmsclient++/typedefs.h>
#include <xmmsclient/xmmsclient++/signal.h>
#include <xmmsclient/xmmsclient++/helpers.h>

#include <list>
#include <string>

namespace Xmms 
{

	class Client 
	{

		public:

			// Constructors
			Client( const std::string& name );

			// Destructor
			virtual ~Client();

			// Connection

			void connect( const std::string& ipcpath = "" );

			// Control
			void quit();


			void
			broadcastQuit( const UintSlot& slot,
			               const ErrorSlot& error = &Xmms::dummy_error );

			// Subsystems

			const Playback playback;
			const Playlist playlist;
			const Medialib medialib;
			const Config   config;
			const Stats    stats;

			// Get an object to create an async main loop
			MainloopInterface& getMainLoop();
			void setMainloop( MainloopInterface* ml );

			bool isConnected() const;

			std::string getLastError() const;

			// Return the internal connection pointer
			inline xmmsc_connection_t* getConnection() const { return conn_; }

		private:
			// Copy-constructor / operator=
			// prevent copying
			Client( const Client& );
			const Client operator=( const Client& ) const;

			bool quitHandler( const unsigned int& time );

			std::string name_;

			xmmsc_connection_t* conn_;

			bool connected_;

			MainloopInterface* mainloop_;
			Listener* listener_;

			Signal<unsigned int>* quitSignal_;

	};

}

#endif // XMMSCLIENTPP_CLIENT_H
