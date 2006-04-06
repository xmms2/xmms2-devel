#include <xmmsclient/xmmsclient.h>
#include <xmmsclient/xmmsclient++/client.h>
#include <xmmsclient/xmmsclient++/playback.h>
#include <xmmsclient/xmmsclient++/helpers.h>
#include <xmmsclient/xmmsclient++/mainloop.h>

#include <boost/bind.hpp>

namespace Xmms
{
	
	Playback::~Playback()
	{
	}

	void Playback::tickle() const
	{

		vCall( connected_, ml_,
		      boost::bind( xmmsc_playback_tickle, conn_ ) );

	}

	void Playback::stop() const
	{

		vCall( connected_, ml_,
		      boost::bind( xmmsc_playback_stop, conn_ ) );
		
	}

	void Playback::pause() const
	{

		vCall( connected_, ml_,
		      boost::bind( xmmsc_playback_pause, conn_ ) );

	}

	void Playback::start() const
	{

		vCall( connected_, ml_,
		       boost::bind( xmmsc_playback_start, conn_ ) );

	}

	Playback::Status Playback::getStatus() const
	{

		xmmsc_result_t* res = 
		    call( connected_, ml_,
		          boost::bind( xmmsc_playback_status, conn_ ) );

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
