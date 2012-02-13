/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2012 XMMS2 Team
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

#ifndef XMMSCLIENTPP_CLIENT_H
#define XMMSCLIENTPP_CLIENT_H

#include <xmmsclient/xmmsclient.h>

#include <xmmsclient/xmmsclient++/playback.h>
#include <xmmsclient/xmmsclient++/xform.h>
#include <xmmsclient/xmmsclient++/playlist.h>
#include <xmmsclient/xmmsclient++/medialib.h>
#include <xmmsclient/xmmsclient++/config.h>
#include <xmmsclient/xmmsclient++/stats.h>
#include <xmmsclient/xmmsclient++/bindata.h>
#include <xmmsclient/xmmsclient++/mainloop.h>
#include <xmmsclient/xmmsclient++/listener.h>
#include <xmmsclient/xmmsclient++/typedefs.h>
#include <xmmsclient/xmmsclient++/signal.h>
#include <xmmsclient/xmmsclient++/collection.h>
#include <xmmsclient/xmmsclient++/result.h>

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
			 *  if ipcpath is omitted or 0, it will try to open
			 *  the default path.
			 *
			 *  @param ipcpath The IPC path. It's broken down like this:
			 *                 @<protocol@>://@<path@>[:@<port@>]. Default is
			 *                 "unix:///tmp/xmms-ipc-@<username@>".
			 *                 - Protocol could be "tcp" or "unix".
			 *                 - Path is either the UNIX socket,
			 *                   or the ipnumber of the server.
			 *                 - Port is only used when the protocol tcp.
			 *  @throw connection_error If connection fails.
			 */
			void connect( const char* ipcpath = 0 );

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
			 *              bool( const int& ).
			 *  @param error Function pointer to an error callback
			 *               function. (<b>optional</b>)
			 *
			 *  @throw connection_error If the client isn't connected.
			 */
			QuitSignal&
			broadcastQuit();

			// Subsystems

			const Bindata    bindata;
			const Playback   playback;
			const Playlist   playlist;
			const Medialib   medialib;
			const Config     config;
			const Stats      stats;
			const Xform      xform;
			const Collection collection;

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

			/** Set disconnection callback.
			 *  
			 *  @param slot A function pointer with function signature void()
			 */
			void
			setDisconnectCallback( const DisconnectCallback::value_type& slot );

			/** Return the connection status.
			 */
			bool isConnected() const;

			/** Return a string that describes the last error.
			 */
			std::string getLastError() const;

			/** Return the internal connection pointer.
			 */
			inline xmmsc_connection_t* getConnection() const { return conn_; }

		/** @cond */
		private:
			// Copy-constructor / operator=
			// prevent copying
			Client( const Client& );
			const Client operator=( const Client& ) const;

			bool quitHandler( const int& time );
			void dcHandler();

			std::string name_;

			xmmsc_connection_t* conn_;

			bool connected_;

			MainloopInterface* mainloop_;
			Listener* listener_;

			QuitSignal* quitSignal_;
			DisconnectCallback* dc_;
		/** @endcond */

	};

}

#endif // XMMSCLIENTPP_CLIENT_H
