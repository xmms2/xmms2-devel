/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2023 XMMS2 Team
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
			DictResult
			mainStats() const;

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
			DictListResult
			pluginList(Plugins::Type type = Plugins::ALL) const;

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
			ReaderStatusSignal
			broadcastMediainfoReaderStatus() const;

			/** Request number of unindexed entries in medialib.
			 *
			 *  @param slot Function pointer to a function taking
			 *              const unsigned int& and returning a bool.
			 *  @param error Function pointer to an error callback
			 *               function. (<b>optional</b>)
			 *
			 *  @throw connection_error If the client isn't connected.
			 */
			IntSignal
			signalMediainfoReaderUnindexed() const;

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
