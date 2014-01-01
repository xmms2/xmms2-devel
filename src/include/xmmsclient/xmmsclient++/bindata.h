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

#ifndef XMMSCLIENTPP_BINDATA_H
#define XMMSCLIENTPP_BINDATA_H

#include <xmmsclient/xmmsclient.h>
#include <xmmsclient/xmmsclient++/mainloop.h>
#include <xmmsclient/xmmsclient++/result.h>

#include <string>

namespace Xmms
{

	class Client;
	/** @class Bindata bindata.h "xmmsclient/xmmsclient++/bindata.h"
	 *  @brief This class handles binary data operations.
	 */
	class Bindata
	{

		public:

			/** Destructor.
			 */
			virtual ~Bindata();

			/** Add binary data to the servers bindata directory.
			 *  
			 *  @param data Binary data to be added.
			 *
			 *  @throw connection_error If the client isn't connected.
			 *  @throw mainloop_running_error If a mainloop is running -
			 *  sync functions can't be called when mainloop is running. This
			 *  is only thrown if the programmer is careless or doesn't know
			 *  what he/she's doing. (logic_error)
			 *  @throw result_error If the operation failed.
			 *
			 *  @return hash of the data which was added.
			 */
			StringResult add( const Xmms::bin& data ) const;

			/** Retrieve binary data from the servers bindata directory,
			 *  based on the hash.
			 *
			 *  @param hash Hash of the binary data to fetch.
			 *
			 *  @throw connection_error If the client isn't connected.
			 *  @throw mainloop_running_error If a mainloop is running -
			 *  sync functions can't be called when mainloop is running. This
			 *  is only thrown if the programmer is careless or doesn't know
			 *  what he/she's doing. (logic_error)
			 *  @throw result_error If the operation failed.
			 *
			 *  @return Binary data which matches the given hash.
			 */
			BinResult retrieve( const std::string& hash ) const;

			/** Remove the binary data associated with the hash.
			 *  
			 *  @param hash Hash of the binary data to remove.
			 *
			 *  @throw connection_error If the client isn't connected.
			 *  @throw mainloop_running_error If a mainloop is running -
			 *  sync functions can't be called when mainloop is running. This
			 *  is only thrown if the programmer is careless or doesn't know
			 *  what he/she's doing. (logic_error)
			 *  @throw result_error If the operation failed.
			 */
			VoidResult remove( const std::string& hash ) const;

			/** List all bindata hashes stored on the server.
			 *
			 *  @throw connection_error If the client isn't connected.
			 *  @throw mainloop_running_error If a mainloop is running -
			 *  sync functions can't be called when mainloop is running. This
			 *  is only thrown if the programmer is careless or doesn't know
			 *  what he/she's doing. (logic_error)
			 *  @throw result_error If the operation failed.
			 */
			StringListResult list() const;

		/** @cond */
		private:

			friend class Client;
			Bindata( xmmsc_connection_t*& conn, bool& connected,
			         MainloopInterface*& ml );

			Bindata( const Bindata& src );
			Bindata operator=( const Bindata& src ) const;

			xmmsc_connection_t*& conn_;
			bool& connected_;
			MainloopInterface*& ml_;
		/** @endcond */

	};

}

#endif // XMMSCLIENTPP_BINDATA_H
