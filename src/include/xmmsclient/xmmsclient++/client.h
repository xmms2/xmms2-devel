#ifndef XMMSCLIENTPP_CLIENT_H
#define XMMSCLIENTPP_CLIENT_H

#include <xmmsclient/xmmsclient.h>

#include <xmmsclient/xmmsclient++/typedefs.h>
#include <xmmsclient/xmmsclient++/playback.h>
#include <xmmsclient/xmmsclient++/playlist.h>
#include <xmmsclient/xmmsclient++/mainloop.h>
#include <xmmsclient/xmmsclient++/listener.h>
#include <xmmsclient/xmmsclient++/typedefs.h>
#include <xmmsclient/xmmsclient++/medialib.h>
#include <xmmsclient/xmmsclient++/signal.h>
#include <xmmsclient/xmmsclient++/helpers.h>

#include <list>
#include <string>

namespace Xmms 
{

	// Types
	namespace Plugins {
		typedef xmms_plugin_type_t Type;
		static const Type ALL = XMMS_PLUGIN_TYPE_ALL;
		static const Type TRANSPORT = XMMS_PLUGIN_TYPE_TRANSPORT;
		static const Type DECODER = XMMS_PLUGIN_TYPE_DECODER;
		static const Type OUTPUT = XMMS_PLUGIN_TYPE_OUTPUT;
		static const Type PLAYLIST = XMMS_PLUGIN_TYPE_PLAYLIST;
		static const Type EFFECT = XMMS_PLUGIN_TYPE_EFFECT;
	}


	class Client 
	{

		public:

			// Constructors
			Client( const std::string& name );

			// Destructor
			virtual ~Client();

			// Commands

			// Connection

			void connect( const std::string& ipcpath = "" );

			// Control
			void quit();

			// stats/info

			const Dict stats() const;

			const DictList pluginList(Plugins::Type type = Plugins::ALL) const;

			void stats(const DictSlot& slot,
			           const ErrorSlot& error = &Xmms::dummy_error ) const;

			void
			stats(const std::list< DictSlot >& slots,
			      const ErrorSlot& error = &Xmms::dummy_error ) const;

			void pluginList(const DictListSlot& slot,
			                const ErrorSlot& error = &Xmms::dummy_error ) const;

			void pluginList(Plugins::Type type,
			                const DictListSlot& slot,
			                const ErrorSlot& error = &Xmms::dummy_error ) const;

			void
			pluginList(const std::list< DictListSlot >& slots,
			           const ErrorSlot& error = &Xmms::dummy_error ) const;

			void
			pluginList(Plugins::Type type,
			           const std::list< DictListSlot >& slots,
			           const ErrorSlot& error = &Xmms::dummy_error ) const;

			void
			broadcastQuit( const UintSlot& slot,
			               const ErrorSlot& error = &Xmms::dummy_error );
			// Subsystems

			const Playback playback;
			const Playlist playlist;
			const Medialib medialib;

			// TODO: write the headers for these and implement them
			//
			// const Config& getConfig() const;

			// Get an object to create an async main loop
			MainLoop& getMainLoop();

			bool isConnected() const;

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

			MainLoop* mainloop_;

			Signal<unsigned int>* quitSignal_;

	};

}

#endif // XMMSCLIENTPP_CLIENT_H
