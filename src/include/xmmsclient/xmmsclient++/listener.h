#ifndef XMMSCLIENTPP_LISTENER_H
#define XMMSCLIENTPP_LISTENER_H

#include <xmmsclient/xmmsclient.h>


namespace Xmms 
{

	class Client;

	// Interface to define MainLoop listeners.
	class ListenerInterface
	{

		public:

			bool operator==( const ListenerInterface& rhs ) const;

		    // Return the file descriptor to listen to
			virtual int32_t getFileDescriptor() const = 0;

		    // Whether to check for data available for reading
			virtual bool listenIn() const = 0;

		    // Whether to check for space available for writing
			virtual bool listenOut() const = 0;

		    // Method run if data available for reading
			virtual void handleIn() = 0;

		    // Method run if space available for writing
			virtual void handleOut() = 0;

	};

	// XMMS2 listener
	class Listener : public ListenerInterface
	{

		public:
			Listener( const Listener& src );
			Listener operator=( const Listener& src ) const;
			virtual ~Listener();

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

#endif // XMMSCLIENTPP_LISTENER_H
