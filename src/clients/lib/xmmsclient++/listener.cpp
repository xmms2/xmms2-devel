#include "listener.h"

namespace Xmms
{

	bool
	ListenerInterface::operator==( const ListenerInterface& rhs ) const
	{
		return ( this->getFileDescriptor == rhs.getFileDescriptor() );
	}


	int32_t
	Listener::getFileDescriptor() const
	{
		return xmmsc_io_fd_get( conn_ );
	}

	bool
	Listener::listenIn() const
	{
		return true;
	}

	bool
	Listener::listenOut() const
	{
		return xmmsc_io_want_out( conn_ );
	}

	void
	Listener::handleIn()
	{
		xmmsc_io_in_handle( conn_ );
	}

	void
	Listener::handleOut()
	{
		xmmsc_io_out_handle( conn_ );
	}

	Listener::Listener( xmmsc_connection_t** conn ) :
		conn_( conn )
	{
	}
}
