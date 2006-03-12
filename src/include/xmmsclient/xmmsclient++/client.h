#ifndef XMMSCLIENTPP_CLIENT_H
#define XMMSCLIENTPP_CLIENT_H

#include <xmmsclient/xmmsclient.h>

#include <xmmsclient/xmmsclient++/typedefs.h>
#include <xmmsclient/xmmsclient++/playback.h>
#include <xmmsclient/xmmsclient++/typedefs.h>

#include <boost/shared_ptr.hpp>

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

			// Commands

			// Connection

			// This could be (optionally, decided with `bool connect` in 
			// constructor) called from constructor
			void connect( const std::string& ipcpath = "" );

			// Destructor also disconnects if neccessary,
			// this could be used to switch to another xmms2d on the fly
			// 
			// disabled for now at least, destructor disconnects anyway
			// and connecting to another ipcpath is easier to implement in
			// connect()
			//
			// void disconnect();

			// Control
			void quit();

			// status/info

			// We can't return a reference to a const HashMap because
			// after the function goes out scope the reference wouldn't
			// be valid anymore so we would have to use a pointer or
			// take a preconstructed ( Xmms::HashMap result; )
			// as function parameter and save the data in it.
			//
			// Optionally: void status( HashMap& result ) const;
			//
			// TODO: Decide what to do about this...
			const DictPtr status() const;

			// Same deal here as above...
			const DictListPtr pluginList() const;

			// Subsystems

			// Other way to do this could be just have the Playback
			// variable as public here, like:
			//
			const Playback playback;
			//
			// User could use it a bit easier (I think)
			//
			// Client client( "myclient" );
			// client.connect();
			// client.playback.start();
			//
			// instead of
			//
			// client.getPlayback().start();
			// const Playback& getPlayback() const;

			// Same thing for all of these:
			// TODO: write the headers for these and implement them
			//
			// const Medialib& getMedialib() const;
			// const Playlist& getPlaylist() const;
			// const Config& getConfig() const;
			//
			// I'm not sure if this should be implemented the same way or not
			// const IO& getIO() const;

		private:
			// Copy-constructor / operator=
			// prevent copying
			Client( const Client& );
			const Client operator=( const Client& ) const;

			std::string name_;

			xmmsc_connection_t* conn_;

			//Playback playback_;

			bool connected_;

	};

}

#endif // XMMSCLIENTPP_CLIENT_H
