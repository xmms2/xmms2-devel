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
			void addListener( const Listener& l );
			void removeListener( const Listener& l );

			// Usage
			void run();

		private:

			list< ListenerInterface > listeners;

			void waitForData();

	};

}

#endif // MAINLOOP_H
