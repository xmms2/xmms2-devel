#ifndef XMMSCLIENTPP_GLIB
#define XMMSCLIENTPP_GLIB

#include <xmmsclient/xmmsclient++/mainloop.h>

namespace Xmms
{

	/** @class GMainloop xmmsclient++-glib.h "xmmsclient/xmmsclient++-glib.h"
	 *  @brief Used to connect xmms2 ipc to glib (and gtkmm) mainloop.
	 */
	class GMainloop : public MainloopInterface
	{

		public:

			/** Constructor, get the connection from Client class.
			 *  @param conn Connection to bind to the glib mainloop.
			 */
			GMainloop( xmmsc_connection_t* conn );

			/** Destructor.
			 *  Disconnects from glib mainloop.
			 */
			~GMainloop();

			/** Doesn't do anything, use glib mainloop functions instead.
			 */
			void run();

		/** @cond */
		private:
			void* data;
		/** @endcond */

	};

}
#endif
