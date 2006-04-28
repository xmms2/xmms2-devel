#include <xmmsclient/xmmsclient++-glib.h>
#include <xmmsclient/xmmsclient-glib.h>

namespace Xmms
{

	GMainloop::GMainloop( xmmsc_connection_t* conn ) :
		MainloopInterface( conn )
	{

		data = xmmsc_mainloop_gmain_init( conn_ );
		running_ = true;

	}

	GMainloop::~GMainloop()
	{

		xmmsc_mainloop_gmain_shutdown( conn_, data );

	}

	void GMainloop::run()
	{
	}

}
