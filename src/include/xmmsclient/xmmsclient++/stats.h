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

	namespace Plugins {
		typedef xmms_plugin_type_t Type;
		static const Type ALL      = XMMS_PLUGIN_TYPE_ALL;
		static const Type OUTPUT   = XMMS_PLUGIN_TYPE_OUTPUT;
		static const Type PLAYLIST = XMMS_PLUGIN_TYPE_PLAYLIST;
		static const Type EFFECT   = XMMS_PLUGIN_TYPE_EFFECT;
		static const Type XFORM    = XMMS_PLUGIN_TYPE_XFORM;
	}


	class Client;

	/** @class Stats stats.h "xmmsclient/xmmsclient++/stats.h"
	 *  @brief This class is used to get various status information from the
	 *         server.
	 */
	class Stats
	{

		public:

			typedef xmms_mediainfo_reader_status_t ReaderStatus;

			/** Mediainfo reader status if it's idling.
			 */
			static const ReaderStatus IDLE
				= XMMS_MEDIAINFO_READER_STATUS_IDLE;

			/** Mediainfo reader status if it's running.
			 */
			static const ReaderStatus RUNNING
				= XMMS_MEDIAINFO_READER_STATUS_RUNNING;


			/** Destructor
			 */
			virtual ~Stats();

			/** Get statistics from the server.
			 *
			 *  @throw connection_error If the client isn't connected.
			 *  @throw mainloop_running_error If a mainloop is running -
			 *  sync functions can't be called when mainloop is running. This
			 *  is only thrown if the programmer is careless or doesn't know
			 *  what he/she's doing. (logic_error)
			 *  @throw result_error If the operation failed.
			 *
			 *  @return Dict containing @e version @c string and @e uptime as
			 *          <code>unsigned int</code>.
			 */
			const Dict mainStats() const;

			/** Get a list of loaded plugins from the server.
			 *
			 *  @param type Type of plugins to get a list of. (<b>optional</b>)
			 *
			 *  @throw connection_error If the client isn't connected.
			 *  @throw mainloop_running_error If a mainloop is running -
			 *  sync functions can't be called when mainloop is running. This
			 *  is only thrown if the programmer is careless or doesn't know
			 *  what he/she's doing. (logic_error)
			 *  @throw result_error If the operation failed.
			 *
			 *  @return List of @link Dict Dicts@endlink containing
			 *          @e name @c string, @e shortname @c string,
			 *          @e version @c string, @e description @c string and
			 *          @e type as <code>Plugins::Type</code>
			 */
			const DictList pluginList(Plugins::Type type = Plugins::ALL) const;

			/** Get statistics from the server.
			 *
			 *  @param slot Function pointer to a function taking
			 *              const Dict& and returning a bool.
			 *  @param error Function pointer to an error callback
			 *               function. (<b>optional</b>)
			 *
			 *  @throw connection_error If the client isn't connected.
			 */
			void
			mainStats(const DictSlot& slot,
			          const ErrorSlot& error = &Xmms::dummy_error ) const;

			/** Get a list of loaded plugins from the server.
			 *
			 *  @param type Type of plugins to get a list of.
			 *  @param slot Function pointer to a function taking
			 *              const std::list< Dict >& and returning a bool.
			 *  @param error Function pointer to an error callback
			 *               function. (<b>optional</b>)
			 *
			 *  @throw connection_error If the client isn't connected.
			 */
			void
			pluginList(Plugins::Type type,
			           const DictListSlot& slot,
			           const ErrorSlot& error = &Xmms::dummy_error ) const;

			/** @overload
			 *  @note This defaults the type to Plugins::ALL
			 */
			void
			pluginList(const DictListSlot& slot,
			           const ErrorSlot& error = &Xmms::dummy_error ) const;

			/** Request the visualisation data signal.
			 *
			 *  @param slot Function pointer to a function taking
			 *              const List<unsigned int>& and returning a bool.
			 *  @param error Function pointer to an error callback
			 *               function. (<b>optional</b>)
			 *
			 *  @throw connection_error If the client isn't connected.
			 */
			void
			signalVisualisationData( const UintListSlot& slot,
			                         const ErrorSlot& error = &Xmms::dummy_error
			                       ) const;

			/** Request status for the mediainfo reader.
			 *
			 *  Compare result with Xmms::Stats::IDLE and Xmms::Stats::RUNNING.
			 *
			 *  @param slot Function pointer to a function taking
			 *              const Xmms::Stats::ReaderStatus& 
			 *              and returning a bool.
			 *  @param error Function pointer to an error callback
			 *               function. (<b>optional</b>)
			 *
			 *  @throw connection_error If the client isn't connected.
			 */
			void
			broadcastMediainfoReaderStatus( const ReaderStatusSlot& slot,
			                                const ErrorSlot& error
			                                      = &Xmms::dummy_error) const;

			/** Request number of unindexed entries in medialib.
			 *
			 *  @param slot Function pointer to a function taking
			 *              const unsigned int& and returning a bool.
			 *  @param error Function pointer to an error callback
			 *               function. (<b>optional</b>)
			 *
			 *  @throw connection_error If the client isn't connected.
			 */
			void
			signalMediainfoReaderUnindexed( const UintSlot& slot,
			                                const ErrorSlot& error
			                                      = &Xmms::dummy_error) const;

		/** @cond */
		private:

			// Constructor, only to be called by Xmms::Client
			friend class Client;
			Stats( xmmsc_connection_t*& conn, bool& connected,
				   MainloopInterface*& ml );

			// Copy-constructor / operator=
			Stats( const Stats& src );
			Stats operator=( const Stats& src ) const;

			xmmsc_connection_t*& conn_;
			bool& connected_;
			MainloopInterface*& ml_;
		/** @endcond */

	};

}

#endif // XMMSCLIENTPP_STATS_H
