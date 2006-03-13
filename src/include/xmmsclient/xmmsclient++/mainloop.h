#ifndef XMMSCLIENTPP_MAINLOOP_H
#define XMMSCLIENTPP_MAINLOOP_H

#include <xmmsclient/xmmsclient++/listener.h>

#include <list>


namespace Xmms 
{

	class MainLoop
	{

		public:

			MainLoop( const MainLoop& src );

			// Destructor
			virtual ~MainLoop();

			// Configuration
			void addListener( ListenerInterface* l );
			void removeListener( ListenerInterface* l );

			// Usage
			void run();

		private:

			// Constructor, only to be called by Xmms::Client
			friend class Client;
			MainLoop();
			MainLoop operator=( const MainLoop& src ) const;

			std::list< ListenerInterface* > listeners;

			void waitForData();

	};

}

#endif // XMMSCLIENTPP_MAINLOOP_H
