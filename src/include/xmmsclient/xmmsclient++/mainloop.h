#ifndef XMMSCLIENTPP_MAINLOOP_H
#define XMMSCLIENTPP_MAINLOOP_H

#include <xmmsclient/xmmsclient++/listener.h>

#include <list>


namespace Xmms 
{

	class MainloopInterface
	{

		public:

			MainloopInterface( xmmsc_connection_t* conn ) :
				running_( false ), conn_( conn ) { }
			virtual ~MainloopInterface() { }

			virtual void run() = 0;

			bool isRunning() const {
				return running_;
			}

		protected:
			bool running_;
			xmmsc_connection_t*& conn_;

	};

	class MainLoop : public MainloopInterface
	{

		public:

			MainLoop( const MainLoop& src );

			// Destructor
			virtual ~MainLoop();

			// Configuration
			void addListener( ListenerInterface* l );
			void removeListener( ListenerInterface* l );

			// Usage
			virtual void run();

		private:

			// Constructor, only to be called by Xmms::Client
			friend class Client;
			MainLoop( xmmsc_connection_t*& conn );
			MainLoop& operator=( const MainLoop& src ) const;

			std::list< ListenerInterface* > listeners;

			void waitForData();

	};

}

#endif // XMMSCLIENTPP_MAINLOOP_H
