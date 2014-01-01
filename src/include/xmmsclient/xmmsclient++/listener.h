/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2014 XMMS2 Team
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

#ifndef XMMSCLIENTPP_LISTENER_H
#define XMMSCLIENTPP_LISTENER_H

#include <xmmsclient/xmmsclient.h>


namespace Xmms 
{

	class Client;

	/** @class ListenerInterface listener.h "xmmsclient/xmmsclient++/listener.h"
	 *  @brief Interface to define MainLoop listeners.
	 */
	class ListenerInterface
	{

		public:

			/** Destructor.
			 */
			virtual ~ListenerInterface() {}

			/** Comparison operator, compares the file descriptors.
			 */
			bool operator==( const ListenerInterface& rhs ) const;

			/** Return the file descriptor of the listener.
			 */
			virtual int32_t getFileDescriptor() const = 0;

			/** Returns whether to check if data is available for reading.
			 */
			virtual bool listenIn() const = 0;

			/** Returns whether to check if space is available for writing.
			 */
			virtual bool listenOut() const = 0;

			/** Method run if data is available for reading.
			 */
			virtual void handleIn() = 0;

			/** Method run if data is available for writing.
			 */
			virtual void handleOut() = 0;

	};

	/** @class Listener listener.h "xmmsclient/xmmsclient++/listener.h"
	 *  @brief XMMS2 Listener for MainLoop.
	 */
	class Listener : public ListenerInterface
	{

		public:
			/** Copy-constructor.
			 */
			Listener( const Listener& src );

			/** Copy assignment operator.
			 */
			Listener& operator=( const Listener& src );

			/** Destructor.
			 */
			virtual ~Listener();

			virtual int32_t getFileDescriptor() const;
			virtual bool listenIn() const;
			virtual bool listenOut() const;
			virtual void handleIn();
			virtual void handleOut();

		/** @cond */
		private:

			// Constructor, only to be called by Xmms::Client
			friend class Client;

			/** Constructor, only callable by Client.
			 *
			 *  @param conn xmmsc_connection_t* of the connection used
			 *              by the listener.
			 */
			Listener( xmmsc_connection_t*& conn );

			xmmsc_connection_t*& conn_;

		/** @endcond */
	};

}

#endif // XMMSCLIENTPP_LISTENER_H
