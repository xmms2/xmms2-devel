#include <xmmsclient/xmmsclient.h>
#include <xmmsclient/xmmsclient++/client.h>
#include <xmmsclient/xmmsclient++/playback.h>
#include <xmmsclient/xmmsclient++/helpers.h>
#include <xmmsclient/xmmsclient++/mainloop.h>

namespace Xmms
{
	
	Playback::~Playback()
	{
	}

	void Playback::tickle() const
	{
		Assert( connected_ );
		Assert( ml_ );

		xmmsc_result_t* res = xmmsc_playback_tickle( conn_ );
		xmmsc_result_wait( res );

		Assert( res );

		xmmsc_result_unref( res );
	}

	void Playback::stop() const
	{
		Assert( connected_ );
		Assert( ml_ );

		xmmsc_result_t* res = xmmsc_playback_stop( conn_ );
		xmmsc_result_wait( res );

		Assert( res );

		xmmsc_result_unref( res );
	}

	void Playback::pause() const
	{
		Assert( connected_ );
		Assert( ml_ );

		xmmsc_result_t* res = xmmsc_playback_pause( conn_ );
		xmmsc_result_wait( res );

		Assert( res );

		xmmsc_result_unref( res );
	}

	void Playback::start() const
	{
		Assert( connected_ );
		Assert( ml_ );

		xmmsc_result_t* res = xmmsc_playback_start( conn_ );
		xmmsc_result_wait( res );

		Assert( res );

		xmmsc_result_unref( res );
	}

	Playback::Status Playback::getStatus() const
	{
		Assert( connected_ );
		Assert( ml_ );

		xmmsc_result_t* res = xmmsc_playback_status( conn_ );
		xmmsc_result_wait( res );

		Assert( res );

		unsigned int status = 0;
		xmmsc_result_get_uint( res, &status );
		xmmsc_result_unref( res );

		return static_cast< Playback::Status >(status);
	}

	Playback::Playback( xmmsc_connection_t*& conn, bool& connected,
	                    MainLoop*& ml ) :
		conn_( conn ), connected_( connected ), ml_( ml )
	{
	}

}
