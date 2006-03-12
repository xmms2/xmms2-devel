#include <xmmsclient/xmmsclient++/playback.h>

namespace Xmms
{
	
	Playback::Playback( const Playback& src ) :
		conn_( src.conn_ )
	{
	}

	Playback Playback::operator=( const Playback& src ) const
	{
		return Playback( src );
	}

	Playback::~Playback()
	{
	}

	void Playback::tickle() const
	{
		xmmsc_result_t* res = xmmsc_playback_tickle( *conn_ );
		xmmsc_result_wait( res );
		xmmsc_result_unref( res );
	}

	void Playback::stop() const
	{
		xmmsc_result_t* res = xmmsc_playback_stop( *conn_ );
		xmmsc_result_wait( res );
		xmmsc_result_unref( res );
	}

	void Playback::pause() const
	{
		xmmsc_result_t* res = xmmsc_playback_pause( *conn_ );
		xmmsc_result_wait( res );
		xmmsc_result_unref( res );
	}

	void Playback::start() const
	{
		xmmsc_result_t* res = xmmsc_playback_start( *conn_ );
		xmmsc_result_wait( res );
		xmmsc_result_unref( res );
	}

	xmms_playback_status_t Playback::getStatus() const
	{
		xmmsc_result_t* res = xmmsc_playback_status( *conn_ );
		xmmsc_result_wait( res );
		unsigned int status = 0;
		xmmsc_result_get_uint( res, &status );
		return static_cast< xmms_playback_status_t >(status);
	}

	Playback::Playback( xmmsc_connection_t** conn ) :
		conn_( conn )
	{
	}

}
