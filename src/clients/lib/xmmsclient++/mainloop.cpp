/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2011 XMMS2 Team
 *
 *  PLUGINS ARE NOT CONSIDERED TO BE DERIVED WORK !!!
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 */

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
		listeners.clear();
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

}
