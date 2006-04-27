#ifndef XMMSCLIENTPP_STATS_H
#define XMMSCLIENTPP_STATS_H

#include <xmmsclient/xmmsclient.h>
#include <xmmsclient/xmmsclient++/mainloop.h>
#include <xmmsclient/xmmsclient++/signal.h>
#include <xmmsclient/xmmsclient++/helpers.h>
#include <xmmsclient/xmmsclient++/typedefs.h>

#include <list>

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


	class Client;
	class Stats
	{

		public:

			typedef xmms_mediainfo_reader_status_t ReaderStatus;

			static const ReaderStatus IDLE    = XMMS_MEDIAINFO_READER_STATUS_IDLE;
			static const ReaderStatus RUNNING = XMMS_MEDIAINFO_READER_STATUS_RUNNING;


			// Destructor
			virtual ~Stats();

			// Commands

			const Dict mainStats() const;

			const DictList pluginList(Plugins::Type type = Plugins::ALL) const;

			void
            mainStats(const DictSlot& slot,
			          const ErrorSlot& error = &Xmms::dummy_error ) const;

			void
			mainStats(const std::list< DictSlot >& slots,
			          const ErrorSlot& error = &Xmms::dummy_error ) const;

			void
            pluginList(const DictListSlot& slot,
			           const ErrorSlot& error = &Xmms::dummy_error ) const;

			void
			pluginList(Plugins::Type type,
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
            signalVisualisationData( const UintListSlot& slot,
			                         const ErrorSlot& error = &Xmms::dummy_error
			                       ) const;

			void
			signalVisualisationData( const std::list< UintListSlot >& slots,
			                         const ErrorSlot& error = &Xmms::dummy_error
			                       ) const;

			void
			broadcastMediainfoReaderStatus( const ReaderStatusSlot& slot,
			                                const ErrorSlot& error
			                                      = &Xmms::dummy_error) const;

			void
			broadcastMediainfoReaderStatus( const std::list< ReaderStatusSlot >&,
			                                const ErrorSlot& error
			                                      = &Xmms::dummy_error) const;

			void
			signalMediainfoReaderUnindexed( const UintSlot& slot,
			                                const ErrorSlot& error
			                                      = &Xmms::dummy_error) const;

			void
			signalMediainfoReaderUnindexed( const std::list< UintSlot>& slots,
			                                const ErrorSlot& error
			                                      = &Xmms::dummy_error) const;

		private:

			// Constructor, only to be called by Xmms::Client
			friend class Client;
			Stats( xmmsc_connection_t*& conn, bool& connected,
				   MainLoop*& ml );

			// Copy-constructor / operator=
			Stats( const Stats& src );
			Stats operator=( const Stats& src ) const;

			xmmsc_connection_t*& conn_;
			bool& connected_;
			MainLoop*& ml_;

	};

}

#endif // XMMSCLIENTPP_STATS_H
