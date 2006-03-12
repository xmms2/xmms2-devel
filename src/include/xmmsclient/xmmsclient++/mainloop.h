#ifndef MAINLOOP_H
#define MAINLOOP_H

#include <xmmsclient/xmmsclient++/listener.h>

#include <list>
using std::list;


namespace Xmms 
{

	class MainLoop
	{

		public:

			MainLoop();
			MainLoop( const MainLoop& src );
			MainLoop operator=( const MainLoop& src ) const;

			// Destructor
			virtual ~MainLoop();

			// Configuration
			void addListener( Listener* l );
			void removeListener( Listener* l );

			// Usage
			void run();

		private:

			// FIXME: Can't we avoid pointers somehow? :-(
			list< ListenerInterface* > listeners;

			void waitForData();

	};

}

#endif // MAINLOOP_H
