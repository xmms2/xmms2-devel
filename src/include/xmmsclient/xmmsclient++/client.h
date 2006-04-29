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

	/** @class Client client.h "xmmsclient/xmmsclient++/client.h"
	 *  @brief This class is used to control everything through various
	 *         Subsystems.
	 *  
	 *  You can access the subsystems directly from the public data fields
	 *  described above.
	 */
	class Client 
	{

		public:

			/** Constructor.
			 *  Constructs client object.
			 *
			 *  @param name Name of the client. Accepts only characters
			 *              in range [a-zA-Z0-9].
			 *
			 *  @todo Should throw std::bad_alloc maybe on error.
			 *
			 */              
			Client( const std::string& name );

			/** Destructor.
			 *  Cleans up everything.
			 */
			virtual ~Client();

			/** Connects to the XMMS2 server.
			 *  if ipcpath is omitted or empty (""), it will try to open
			 *  the default path.
			 *
			 *  @param ipcpath The IPC path. It's broken down like this:
			 *                 <protocol>://<path>[:<port>]. Default is
			 *                 "unix:///tmp/xmms-ipc-<username>".
			 *                 - Protocol could be "tcp" or "unix".
			 *                 - Path is either the UNIX socket,
			 *                   or the ipnumber of the server.
			 *                 - Port is only used when the protocol tcp.
			 *  @throw connection_error If connection fails.
			 */
			void connect( const std::string& ipcpath = "" );

			/** Tell the server to quit.
			 *  This will terminate the server. Destruct this object if you
			 *  just want to disconnect.
			 */
			void quit();

			/** Request the quit broadcast.
			 *  The callback will be called when the server is terminating.
			 *
			 *  @param slot Function pointer to the callback function.
			 *              Function signature must be
			 *              bool( const unsigned int& ).
			 *  @param error Function pointer to an error callback
			 *               function. (<b>optional</b>)
			 *
			 *  @throw connection_error If the client isn't connected.
			 */
			void
			broadcastQuit( const UintSlot& slot,
			               const ErrorSlot& error = &Xmms::dummy_error );

			// Subsystems

			const Playback playback;
			const Playlist playlist;
			const Medialib medialib;
			const Config   config;
			const Stats    stats;

			/** Get the current mainloop.
			 *  If no mainloop is set, it will create a default MainLoop.
			 *  
			 *  @return Reference to the current mainloop object.
			 */
			MainloopInterface& getMainLoop();

			/** Set the mainloop which is to be used.
			 *  
			 *  @param ml A mainloop class derived from MainloopInterface.
			 *
			 *  @note The parameter @b must be created with <i>new</i>,
			 *        and it must @b not be destructed at any point.
			 *        The Client class will take care of its destruction.
			 */
			void setMainloop( MainloopInterface* ml );

			/** Return the connection status.
			 */
			bool isConnected() const;

			/** Returns a string that describes the last error.
			 */
			std::string getLastError() const;

			// Return the internal connection pointer
			inline xmmsc_connection_t* getConnection() const { return conn_; }

		/** @cond */
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
		/** @endcond */

	};

}

#endif // XMMSCLIENTPP_CLIENT_H
