#include <xmmsclient/xmmsclient++/mainloop.h>
#include <xmmsclient/xmmsclient++/listener.h>
#include <xmmsclient/xmmsclient.h>

#include <list>
using std::list;


namespace Xmms
{

	MainLoop::MainLoop( xmmsc_connection_t*& conn )
		: MainloopInterface( conn ), listeners()
	{
	}

	MainLoop::~MainLoop()
	{
		list< ListenerInterface* >::iterator lit;
		for(lit = listeners.begin(); lit != listeners.end(); ++lit) {
			delete (*lit);
		}
	}

	void
	MainLoop::addListener( ListenerInterface* l )
	{
		listeners.push_back( l );
	}

	void
	MainLoop::removeListener( ListenerInterface* l )
	{
		listeners.remove( l );
	}

	void
	MainLoop::run()
	{
		running_ = true;
		while(listeners.size() > 0) {
			waitForData();
		}
	}

	void
	MainLoop::waitForData()
	{
		fd_set rfds, wfds;
		int modfds( 0 );
		int maxfds( -1 );

		list< ListenerInterface* >::iterator lit;

		// Setup fds
		FD_ZERO(&rfds);
		FD_ZERO(&wfds);

		for(lit = listeners.begin(); lit != listeners.end(); ++lit) {
			if( (*lit)->listenOut() ) {
				FD_SET( (*lit)->getFileDescriptor(), &wfds);

				if( (*lit)->getFileDescriptor() > maxfds ) {
					maxfds = (*lit)->getFileDescriptor();
				}
			}

			if( (*lit)->listenIn() ) {
				FD_SET( (*lit)->getFileDescriptor(), &rfds);

				if( (*lit)->getFileDescriptor() > maxfds ) {
					maxfds = (*lit)->getFileDescriptor();
				}
			}
		}

		// Select on the fds
		if( maxfds >= 0 ) {
			modfds = select(maxfds + 1, &rfds, &wfds, NULL, NULL);
		}

		if(modfds < 0) {
			// FIXME: Error
		}
		// Handle the data
		else if(modfds > 0) {
			for(lit = listeners.begin();
			    lit != listeners.end() && listeners.size() != 0;
			    ++lit) {

				if( (*lit)->listenOut()
					&& FD_ISSET((*lit)->getFileDescriptor(), &wfds) ) {
					(*lit)->handleOut();
				}

				if( (*lit)->listenIn()
					&& FD_ISSET((*lit)->getFileDescriptor(), &rfds) ) {
					(*lit)->handleIn();
				}

			}
		}
	}

	bool
	MainLoop::isRunning() const
	{
		return running_;
	}

}
