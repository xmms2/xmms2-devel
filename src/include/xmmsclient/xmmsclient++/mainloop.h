#ifndef XMMSCLIENTPP_MAINLOOP_H
#define XMMSCLIENTPP_MAINLOOP_H

#include <xmmsclient/xmmsclient++/listener.h>

#include <list>
using std::list;


namespace Xmms 
{

	// FIXME: Why isn't the header inclusion working?
	class ListenerInterface;

	class MainLoop
	{

		public:

			MainLoop();
			MainLoop( const MainLoop& src );
			MainLoop operator=( const MainLoop& src ) const;

			// Destructor
			virtual ~MainLoop();

			// Configuration
			void addListener( ListenerInterface* l );
			void removeListener( ListenerInterface* l );

			// Usage
			void run();

		private:

			// FIXME: Can't we avoid pointers somehow? :-(
			list< ListenerInterface* > listeners;

			void waitForData();

	};

}

#endif // XMMSCLIENTPP_MAINLOOP_H
