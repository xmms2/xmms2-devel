#ifndef XMMSCLIENTPP_GLIB
#define XMMSCLIENTPP_GLIB

#include <xmmsclient/xmmsclient++/mainloop.h>

namespace Xmms
{

	class GMainloop : public MainloopInterface
	{

		public:

			GMainloop( xmmsc_connection_t* conn );
			~GMainloop();

			void run();

		private:
			void* data;

	};

}
#endif
