#include "mainloop.h"

namespace Xmms
{

	MainLoop::MainLoop()
		: listeners()
	{
	}

	MainLoop::MainLoop( const MainLoop& src )
		: listeners( src.listeners )
	{
	}

	MainLoop MainLoop::operator=( const MainLoop& src ) const
	{
		return MainLoop( src );
	}

	MainLoop::~MainLoop()
	{
	}

	void
	MainLoop::addListener( const Listener& l )
	{
		listeners.push_back( l );
	}

	void
	MainLoop::removeListener( const Listener& l )
	{
		listeners.remove( l );
	}

	void
	MainLoop::run()
	{
		while(true) {
			waitForData();
		}
	}

	void
	MainLoop::waitForData()
	{
		fd_set rfds, wfds;
		int modfds;

		list< ListenerInterface >::iterator lit;

		// Setup fds
		FD_ZERO(&rfds);
		FD_ZERO(&wfds);

		for(lit = listeners.begin(); lit != listeners.end(); ++lit) {
			if( (*lit).listenIn() ) {
				FD_SET( (*lit).getFileDescriptor(), &rfds);
			}

			if( (*lit).listenOut() ) {
				FD_SET( (*lit).getFileDescriptor(), &wfds);
			}
		}

		// Select on the fds
		modfds = select(xmmsIpc + 1, &rfds, &wfds, NULL, NULL);

		if(modfds < 0) {
			// FIXME: Error
		}
		// Handle the data
		else if(modfds > 0) {
			for(lit = listeners.begin(); lit != listeners.end(); ++lit) {
				if( (*lit).listenOut()
					&& FD_ISSET((*lit).getFileDescriptor(), &rfds) ) {
					(*lit).handleIn();
				}

				if( (*lit).listenOut()
					&& FD_ISSET((*lit).getFileDescriptor(), &rfds) ) {
					(*lit).handleOut();
				}
			}
		}
	}

}
