#ifndef XMMSCLIENTPP_XFORM_H
#define XMMSCLIENTPP_XFORM_H

#include <xmmsclient/xmmsclient.h>
#include <xmmsclient/xmmsclient++/mainloop.h>
#include <xmmsclient/xmmsclient++/dict.h>
#include <xmmsclient/xmmsclient++/list.h>
#include <xmmsclient/xmmsclient++/typedefs.h>
#include <xmmsclient/xmmsclient++/helpers.h>
#include <xmmsclient/xmmsclient++/signal.h>

#include <string>

namespace Xmms
{

	class Client;

	/** @class Xform xform.h "xmmsclient/xmmsclient++/xform.h"
	 *  @brief This class controls the xform object.
	 */
	class Xform
	{

		public:
			/** Destructor. */
			~Xform();

			/** Browse available media in a path.
			 *  A list of paths available (directly) under the
			 *  specified path is returned.
			 *
			 *  @param url Path to browse.
			 *
			 *  @throw connection_error If the client isn't connected.
			 *  @throw mainloop_running_error If a mainloop is running -
			 *  sync functions can't be called when mainloop is running. This
			 *  is only thrown if the programmer is careless or doesn't know
			 *  what he/she's doing. (logic_error)
			 *  @throw result_error If the operation failed.
			 *
			 *  @return a list of information dict for each path.
			 */
			List< Dict > browse( const std::string& url ) const;

			/** Browse available media in a path.
			 *  Same as #browse but takes an encoded path instead.
			 *
			 *  @param url Encoded path to browse.
			 *
			 *  @throw connection_error If the client isn't connected.
			 *  @throw mainloop_running_error If a mainloop is running -
			 *  sync functions can't be called when mainloop is running. This
			 *  is only thrown if the programmer is careless or doesn't know
			 *  what he/she's doing. (logic_error)
			 *  @throw result_error If the operation failed.
			 *
			 *  @return a list of information dict for each path.
			 */
			List< Dict > browseEncoded( const std::string& url ) const;


			/** Browse available media in a path.
			 *  A list of paths available (directly) under the
			 *  specified path is returned.
			 *
			 *  @param url Path to browse.
			 *  @param slot Function pointer to a function taking a
			 *              const List< Dict >& and returning a bool.
			 *  @param error Function pointer to an error callback
			 *               function. (<b>optional</b>)
			 *
			 *  @throw connection_error If the client isn't connected.
			 */
			void browse( const std::string& url, const DictListSlot& slot,
			             const ErrorSlot& error = &Xmms::dummy_error ) const;

			/** Browse available media in a path.
			 *  Same as #browse but takes an encoded path instead.
			 *
			 *  @param url Encoded path to browse.
			 *  @param slot Function pointer to a function taking a
			 *              const List< Dict >& and returning a bool.
			 *  @param error Function pointer to an error callback
			 *               function. (<b>optional</b>)
			 *
			 *  @throw connection_error If the client isn't connected.
			 */
			void browseEncoded( const std::string& url, const DictListSlot& slot,
			                    const ErrorSlot& error = &Xmms::dummy_error
			                  ) const;

		/** @cond */
		private:
			friend class Client;
			Xform ( xmmsc_connection_t*& conn, bool& connected,
					MainloopInterface*& ml );

			Xform( const Xform& src );
			Xform operator=( const Xform& src );

			xmmsc_connection_t*& conn_;
			bool& connected_;
			MainloopInterface*& ml_;
		/** @endcond */
	};
}

#endif
