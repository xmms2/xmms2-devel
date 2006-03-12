#ifndef LISTENER_H
#define LISTENER_H

#include <xmmsclient/xmmsclient++/client.h>


namespace Xmms 
{

	// Interface to define MainLoop listeners.
	class ListenerInterface
	{

		public:
			bool operator==( const ListenerInterface& rhs ) const;

		    // Return the file descriptor to listen to
			virtual int32_t getFileDescriptor() const;

		    // Whether to check for data available for reading
			virtual bool listenIn() const;

		    // Whether to check for space available for writing
			virtual bool listenOut() const;

		    // Method run if data available for reading
			virtual void handleIn();

		    // Method run if space available for writing
			virtual void handleOut();

	};

	// XMMS2 listener
	class Listener : public ListenerInterface
	{

		public:

			virtual int32_t getFileDescriptor() const;
			virtual bool listenIn() const;
			virtual bool listenOut() const;
			virtual void handleIn();
			virtual void handleOut();

		private:

			// Constructor, only to be called by Xmms::Client
			friend class Client;
			Listener( xmmsc_connection_t** conn );

			xmmsc_connection_t** conn_;

	};

}

#endif // LISTENER_H
