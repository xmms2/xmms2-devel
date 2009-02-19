/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2009 XMMS2 Team
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

#ifndef XMMSCLIENTPP_PLAYLIST_HH
#define XMMSCLIENTPP_PLAYLIST_HH

#include <xmmsclient/xmmsclient.h>
#include <xmmsclient/xmmsclient++/mainloop.h>
#include <xmmsclient/xmmsclient++/result.h>
#include <xmmsclient/xmmsclient++/coll.h>
#include <string>
#include <list>

namespace Xmms
{
	
	class Client;

	/** @class Playlist playlist.h "xmmsclient/xmmsclient++/playlist.h"
	 *  @brief This class controls the playlist
	 */
	class Playlist
	{

		public:

			static const std::string DEFAULT_PLAYLIST;


			/** Destructor. 
			 *  Doesn't really do anything at this moment :) 
			 */
			virtual ~Playlist();

			/** Get the list of saved playlists.
			 *
			 *  @throw connection_error If the client isn't connected.
			 *  @throw mainloop_running_error If a mainloop is running -
			 *  sync functions can't be called when mainloop is running. This
			 *  is only thrown if the programmer is careless or doesn't know
			 *  what he/she's doing. (logic_error)
			 *  @throw result_error If the operation failed.
			 */
			StringListResult list() const;

			/** Create a new empty playlist.
			 *
			 *  @param playlist the name of the playlist to create.
			 *
			 *  @throw connection_error If the client isn't connected.
			 *  @throw mainloop_running_error If a mainloop is running -
			 *  sync functions can't be called when mainloop is running. This
			 *  is only thrown if the programmer is careless or doesn't know
			 *  what he/she's doing. (logic_error)
			 *  @throw result_error If the operation failed.
			 */
			VoidResult create( const std::string& playlist ) const;

			/** Load a saved playlist and make it the active playlist.
			 *
			 *  @param playlist the playlist to load.
			 *
			 *  @throw connection_error If the client isn't connected.
			 *  @throw mainloop_running_error If a mainloop is running -
			 *  sync functions can't be called when mainloop is running. This
			 *  is only thrown if the programmer is careless or doesn't know
			 *  what he/she's doing. (logic_error)
			 *  @throw result_error If the operation failed.
			 */
			VoidResult load( const std::string& playlist ) const;

			/**	Add the url to a playlist.
			 *  The url should be absolute to the server-side. 
			 *  Note that you will have to include the protocol 
			 *  for the url to. ie: file://mp3/my_mp3s/first.mp3.
			 *  
			 *  @param url file to be added
			 *  @param playlist the playlist to modify (if omitted,
			 *                  act on the current playlist)
			 *
			 *  @throw connection_error If the client isn't connected.
			 *  @throw mainloop_running_error If a mainloop is running -
			 *  sync functions can't be called when mainloop is running. This
			 *  is only thrown if the programmer is careless or doesn't know
			 *  what he/she's doing. (logic_error)
			 *  @throw result_error If the operation failed.
			 */
			VoidResult addUrl( const std::string& url,
			                   const std::string& playlist = DEFAULT_PLAYLIST ) const;

			/** Add the url with arguments to a playlist.
			 *  The url should be absolute to the server-side.
			 *  Note that you will have to include the protocol
			 *  for the url to. ie: file://mp3/my_mp3s/first.mp3.
			 *
			 *  @param url file to be added
			 *  @param args List of strings used as arguments.
			 *  @param playlist the playlist to modify (if omitted,
			 *                  act on the current playlist)
			 *
			 *  @throw connection_error If the client isn't connected.
			 *  @throw mainloop_running_error If a mainloop is running -
			 *  sync functions can't be called when mainloop is running. This
			 *  is only thrown if the programmer is careless or doesn't know
			 *  what he/she's doing. (logic_error)
			 *  @throw result_error If the operation failed.
			 */
			VoidResult addUrl( const std::string& url,
			                   const std::list< std::string>& args,
			                   const std::string& playlist = DEFAULT_PLAYLIST ) const;

			/**	Add the url to a playlist.
			 *  Same as #addUrl but takes an encoded URL instead
			 *  
			 *  @param url file to be added
			 *  @param playlist the playlist to modify (if omitted,
			 *                  act on the current playlist)
			 *
			 *  @throw connection_error If the client isn't connected.
			 *  @throw mainloop_running_error If a mainloop is running -
			 *  sync functions can't be called when mainloop is running. This
			 *  is only thrown if the programmer is careless or doesn't know
			 *  what he/she's doing. (logic_error)
			 *  @throw result_error If the operation failed.
			 */
			VoidResult addUrlEncoded( const std::string& url,
			                          const std::string& playlist = DEFAULT_PLAYLIST
			                        ) const;

			/**	Add the directory recursively to a playlist.
			 *  The url should be absolute to the server-side. 
			 *  Note that you will have to include the protocol 
			 *  for the url to. ie: file://mp3/my_mp3s/directory
			 *  
			 *  @param url directory to be added
			 *  @param playlist the playlist to modify (if omitted,
			 *                  act on the current playlist)
			 *
			 *  @throw connection_error If the client isn't connected.
			 *  @throw mainloop_running_error If a mainloop is running -
			 *  sync functions can't be called when mainloop is running. This
			 *  is only thrown if the programmer is careless or doesn't know
			 *  what he/she's doing. (logic_error)
			 *  @throw result_error If the operation failed.
			 */
			VoidResult addRecursive( const std::string& url,
			                         const std::string& playlist = DEFAULT_PLAYLIST
			                       ) const;

			/**	Add the directory recursivly to a playlist.
			 *  Same as #addRecursive but takes a encoded URL instead.
			 *  
			 *  @param url directory to be added
			 *  @param playlist the playlist to modify (if omitted,
			 *                  act on the current playlist)
			 *
			 *  @throw connection_error If the client isn't connected.
			 *  @throw mainloop_running_error If a mainloop is running -
			 *  sync functions can't be called when mainloop is running. This
			 *  is only thrown if the programmer is careless or doesn't know
			 *  what he/she's doing. (logic_error)
			 *  @throw result_error If the operation failed.
			 */
			VoidResult addRecursiveEncoded( const std::string& url,
			                                const std::string& playlist
			                                             = DEFAULT_PLAYLIST
			                              ) const;

			/** Add a medialib id to a playlist.
			 *
			 *  @param id A medialib ID
			 *  @param playlist the playlist to modify (if omitted,
			 *                  act on the current playlist)
			 *
			 *  @throw connection_error If the client isn't connected.
			 *  @throw mainloop_running_error If a mainloop is running -
			 *  sync functions can't be called when mainloop is running. This
			 *  is only thrown if the programmer is careless or doesn't know
			 *  what he/she's doing. (logic_error)
			 *  @throw result_error If the operation failed.
			 */
			VoidResult addId( const unsigned int id,
			                  const std::string& playlist = DEFAULT_PLAYLIST ) const;

			/** Add an idlist to a playlist.
			 *
			 *  @param idlist an ID list
			 *  @param playlist the playlist to modify (if omitted,
			 *                  act on the current playlist)
			 *
			 *  @throw connection_error If the client isn't connected.
			 *  @throw mainloop_running_error If a mainloop is running -
			 *  sync functions can't be called when mainloop is running. This
			 *  is only thrown if the programmer is careless or doesn't know
			 *  what he/she's doing. (logic_error)
			 *  @throw result_error If the operation failed.
			 *  @throw std::bad_cast If idlist is not a valid const Coll::Idlist&.
			 */
			VoidResult addIdlist( const Coll::Coll& idlist,
			                      const std::string& playlist = DEFAULT_PLAYLIST ) const;

			/**	Add the content of the given collection to a playlist.
			 *  The list of ordering properties defines how the set of
			 *  matching media is added.
			 *
			 *  @param collection The collection from which media will
			 *                    be added to the playlist.
			 *  @param order The order in which the matched media are added.
			 *  @param playlist the playlist to modify (if omitted,
			 *                  act on the current playlist)
			 *
			 *  @throw connection_error If the client isn't connected.
			 *  @throw mainloop_running_error If a mainloop is running -
			 *  sync functions can't be called when mainloop is running. This
			 *  is only thrown if the programmer is careless or doesn't know
			 *  what he/she's doing. (logic_error)
			 *  @throw result_error If the operation failed.
			 */
			VoidResult addCollection( const Coll::Coll& collection,
			                          const std::list< std::string >& order,
			                          const std::string& playlist = DEFAULT_PLAYLIST ) const;

			/** Clears a playlist.
			 *
			 *  @param playlist the playlist to modify (if omitted,
			 *                  act on the current playlist)
			 *
			 *  @throw connection_error If the client isn't connected.
			 *  @throw mainloop_running_error If a mainloop is running -
			 *  sync functions can't be called when mainloop is running. This
			 *  is only thrown if the programmer is careless or doesn't know
			 *  what he/she's doing. (logic_error)
			 *  @throw result_error If the operation failed.
			 */
			VoidResult clear( const std::string& playlist = DEFAULT_PLAYLIST ) const;

			/** Retrieve the current position in a playlist.
			 *
			 *  @param playlist the playlist to consider (if omitted,
			 *                  act on the current playlist)
			 *
			 *  @throw connection_error If the client isn't connected.
			 *  @throw mainloop_running_error If a mainloop is running -
			 *  sync functions can't be called when mainloop is running. This
			 *  is only thrown if the programmer is careless or doesn't know
			 *  what he/she's doing. (logic_error)
			 *  @throw result_error If the operation failed.
			 *
			 *  @return Dict containing then name of a playlist in 'name' and
			 *  the current position in that playlist as unsigned integer in
			 *  'position'.
			 */
			DictResult currentPos( const std::string& playlist = DEFAULT_PLAYLIST
			                     ) const;

			/** Retrieve the name of the current active playlist.
			 *
			 *  @throw connection_error If the client isn't connected.
			 *  @throw mainloop_running_error If a mainloop is running -
			 *  sync functions can't be called when mainloop is running. This
			 *  is only thrown if the programmer is careless or doesn't know
			 *  what he/she's doing. (logic_error)
			 *  @throw result_error If the operation failed.
			 *
			 *  @return current position as unsigned integer.
			 */
			StringResult currentActive() const;

			/** Insert an entry at given position in a playlist.
			 *  
			 *  @param pos A position in the playlist.
			 *  @param url The URL to insert.
			 *  @param playlist the playlist to modify (if omitted,
			 *                  act on the current playlist)
			 *
			 *  @throw connection_error If the client isn't connected.
			 *  @throw mainloop_running_error If a mainloop is running -
			 *  sync functions can't be called when mainloop is running. This
			 *  is only thrown if the programmer is careless or doesn't know
			 *  what he/she's doing. (logic_error)
			 *  @throw result_error If the operation failed.
			 */
			VoidResult insertUrl( int pos, const std::string& url,
			                      const std::string& playlist = DEFAULT_PLAYLIST
			                    ) const;

			/** Insert an entry with args at given position in a playlist.
			 *
			 *  @param pos A position in the playlist.
			 *  @param url The URL to insert.
			 *  @param args List of strings used as arguments.
			 *  @param playlist the playlist to modify (if omitted,
			 *                  act on the current playlist)
			 *
			 *  @throw connection_error If the client isn't connected.
			 *  @throw mainloop_running_error If a mainloop is running -
			 *  sync functions can't be called when mainloop is running. This
			 *  is only thrown if the programmer is careless or doesn't know
			 *  what he/she's doing. (logic_error)
			 *  @throw result_error If the operation failed.
			 */
			VoidResult insertUrl( int pos, const std::string& url,
			                      const std::list< std::string >& args,
			                      const std::string& playlist = DEFAULT_PLAYLIST
			                    ) const;

			/** Insert an entry at given position in a playlist.
			 *  Same as #insertUrl but takes a encoded url instead.
			 *  
			 *  @param pos A position in the playlist.
			 *  @param url The URL to insert.
			 *  @param playlist the playlist to modify (if omitted,
			 *                  act on the current playlist)
			 *
			 *  @throw connection_error If the client isn't connected.
			 *  @throw mainloop_running_error If a mainloop is running -
			 *  sync functions can't be called when mainloop is running. This
			 *  is only thrown if the programmer is careless or doesn't know
			 *  what he/she's doing. (logic_error)
			 *  @throw result_error If the operation failed.
			 */
			VoidResult insertUrlEncoded( int pos, const std::string& url,
			                             const std::string& playlist = DEFAULT_PLAYLIST
			                           ) const;

			/** Insert a medialib ID at given position in a playlist.
			 *
			 *  @param pos A position in the playlist.
			 *  @param id A medialib ID.
			 *  @param playlist the playlist to modify (if omitted,
			 *                  act on the current playlist)
			 *
			 *  @throw connection_error If the client isn't connected.
			 *  @throw mainloop_running_error If a mainloop is running -
			 *  sync functions can't be called when mainloop is running. This
			 *  is only thrown if the programmer is careless or doesn't know
			 *  what he/she's doing. (logic_error)
			 *  @throw result_error If the operation failed.
			 */
			VoidResult insertId( int pos, unsigned int id,
			                     const std::string& playlist = DEFAULT_PLAYLIST
			                   ) const;

			/**	Add the content of the given collection to a playlist.
			 *  The list of ordering properties defines how the set of
			 *  matching media is added.
			 *
			 *  @param pos A position in the playlist.
			 *  @param collection The collection from which media will
			 *                    be added to the playlist.
			 *  @param order The order in which the matched media are added.
			 *  @param playlist the playlist to modify (if omitted,
			 *                  act on the current playlist)
			 *
			 *  @throw connection_error If the client isn't connected.
			 *  @throw mainloop_running_error If a mainloop is running -
			 *  sync functions can't be called when mainloop is running. This
			 *  is only thrown if the programmer is careless or doesn't know
			 *  what he/she's doing. (logic_error)
			 *  @throw result_error If the operation failed.
			 */
			VoidResult insertCollection( int pos, const Coll::Coll& collection,
			                             const std::list< std::string >& order,
			                             const std::string& playlist = DEFAULT_PLAYLIST
			                           ) const;

			/**	Insert the directory recursively at a given position in a
			 *  playlist.
			 *  The url should be absolute to the server-side.
			 *  Note that you will have to include the protocol
			 *  for the url to. ie: file://mp3/my_mp3s/directory
			 *
			 *  @param pos A position in the playlist.
			 *  @param url directory to be added
			 *  @param playlist the playlist to modify (if omitted,
			 *                  act on the current playlist)
			 *
			 *  @throw connection_error If the client isn't connected.
			 *  @throw mainloop_running_error If a mainloop is running -
			 *  sync functions can't be called when mainloop is running. This
			 *  is only thrown if the programmer is careless or doesn't know
			 *  what he/she's doing. (logic_error)
			 *  @throw result_error If the operation failed.
			 */
			VoidResult insertRecursive( int pos, const std::string& url,
			                            const std::string& playlist
			                                         = DEFAULT_PLAYLIST
			                          ) const;

			/**	Insert the directory recursivly at a given position in a
			 *  playlist.
			 *  Same as #insertRecursive but takes a encoded URL instead.
			 *
			 *  @param pos A position in the playlist.
			 *  @param url directory to be added
			 *  @param playlist the playlist to modify (if omitted,
			 *                  act on the current playlist)
			 *
			 *  @throw connection_error If the client isn't connected.
			 *  @throw mainloop_running_error If a mainloop is running -
			 *  sync functions can't be called when mainloop is running. This
			 *  is only thrown if the programmer is careless or doesn't know
			 *  what he/she's doing. (logic_error)
			 *  @throw result_error If the operation failed.
			 */
			VoidResult insertRecursiveEncoded( int pos, const std::string& url,
			                                   const std::string& playlist
			                                             = DEFAULT_PLAYLIST
			                                 ) const;

			/** Retrieve the entries in a playlist.
			 *
			 *  @param playlist the playlist to consider (if omitted,
			 *                  act on the current playlist)
			 *
			 *  @throw connection_error If the client isn't connected.
			 *  @throw mainloop_running_error If a mainloop is running -
			 *  sync functions can't be called when mainloop is running. This
			 *  is only thrown if the programmer is careless or doesn't know
			 *  what he/she's doing. (logic_error)
			 *  @throw result_error If the operation failed.
			 *
			 *  @return A List of medialib IDs
			 */
			IntListResult listEntries( const std::string& playlist
			                                       = DEFAULT_PLAYLIST ) const;

			/** Move a playlist entry to a new position (absolute move).
			 * 
			 *  @param curpos Position of the entry to be moved.
			 *  @param newpos Position where the entry should be moved to.
			 *  @param playlist the playlist to modify (if omitted,
			 *                  act on the current playlist)
			 *
			 *  @throw connection_error If the client isn't connected.
			 *  @throw mainloop_running_error If a mainloop is running -
			 *  sync functions can't be called when mainloop is running. This
			 *  is only thrown if the programmer is careless or doesn't know
			 *  what he/she's doing. (logic_error)
			 *  @throw result_error If the operation failed.
			 */
			VoidResult moveEntry( unsigned int curpos, unsigned int newpos,
			                      const std::string& playlist = DEFAULT_PLAYLIST
			                    ) const;

			/** Remove an entry from a playlist.
			 * 
			 *  @param pos The position that should be 
			 *             removed from the playlist.
			 *  @param playlist the playlist to modify (if omitted,
			 *                  act on the current playlist)
			 *
			 *  @throw connection_error If the client isn't connected.
			 *  @throw mainloop_running_error If a mainloop is running -
			 *  sync functions can't be called when mainloop is running. This
			 *  is only thrown if the programmer is careless or doesn't know
			 *  what he/she's doing. (logic_error)
			 *  @throw result_error If the operation failed.
			 */
			VoidResult removeEntry( unsigned int pos,
			                        const std::string& playlist = DEFAULT_PLAYLIST
			                      ) const;

			/** Removes a playlist.
			 *
			 *  @param playlist the playlist to remove.
			 *
			 *  @throw connection_error If the client isn't connected.
			 *  @throw mainloop_running_error If a mainloop is running -
			 *  sync functions can't be called when mainloop is running. This
			 *  is only thrown if the programmer is careless or doesn't know
			 *  what he/she's doing. (logic_error)
			 *  @throw result_error If the operation failed.
			 */
			VoidResult remove( const std::string& playlist ) const;

			/** Set next entry in the playlist.
			 * 
			 *  @param pos A position to jump to.
			 *
			 *  @throw connection_error If the client isn't connected.
			 *  @throw mainloop_running_error If a mainloop is running -
			 *  sync functions can't be called when mainloop is running. This
			 *  is only thrown if the programmer is careless or doesn't know
			 *  what he/she's doing. (logic_error)
			 *  @throw result_error If the operation failed.
			 */
			IntResult setNext( unsigned int pos ) const;

			/** Same as setNext but relative to the current position.
			 *
			 *  -1 will back one and 1 will move to the next entry.
			 *
			 *  @param pos Relative position for the jump.
			 *
			 *  @throw connection_error If the client isn't connected.
			 *  @throw mainloop_running_error If a mainloop is running -
			 *  sync functions can't be called when mainloop is running. This
			 *  is only thrown if the programmer is careless or doesn't know
			 *  what he/she's doing. (logic_error)
			 *  @throw result_error If the operation failed.
			 */
			IntResult setNextRel( signed int pos ) const;

			/** Shuffles a playlist.
			 *
			 *  @param playlist the playlist to modify (if omitted,
			 *                  act on the current playlist)
			 *
			 *  @throw connection_error If the client isn't connected.
			 *  @throw mainloop_running_error If a mainloop is running -
			 *  sync functions can't be called when mainloop is running. This
			 *  is only thrown if the programmer is careless or doesn't know
			 *  what he/she's doing. (logic_error)
			 *  @throw result_error If the operation failed.
			 */
			VoidResult shuffle( const std::string& playlist = DEFAULT_PLAYLIST
			                  ) const;

			/** Sorts a playlist according to a list of properties.
			 * 
			 *  @param properties Properties to sort by.
			 *  @param playlist the playlist to modify (if omitted,
			 *                  act on the current playlist)
			 *
			 *  @throw connection_error If the client isn't connected.
			 *  @throw mainloop_running_error If a mainloop is running -
			 *  sync functions can't be called when mainloop is running. This
			 *  is only thrown if the programmer is careless or doesn't know
			 *  what he/she's doing. (logic_error)
			 *  @throw result_error If the operation failed.
			 */
			VoidResult sort( const std::list<std::string>& properties,
			                 const std::string& playlist = DEFAULT_PLAYLIST ) const;

			/** Request the playlist changed broadcast from the server.
			 *
			 *  Everytime someone manipulates the playlist this will be emitted.
			 *
			 *  @param slot Function pointer to a function taking a
			 *              const Dict& and returning a bool.
			 *  @param error Function pointer to an error callback
			 *               function. (<b>optional</b>)
			 *
			 *  @throw connection_error If the client isn't connected.
			 */
			DictSignal broadcastChanged() const; 

			/** Request the playlist current position broadcast.
			 *
			 *  When the position in the playlist is
			 *  changed this will be called.
			 *
			 *  @param slot Function pointer to a function taking a
			 *              const unsigned int& and returning a bool.
			 *  @param error Function pointer to an error callback
			 *               function. (<b>optional</b>)
			 *
			 *  @throw connection_error If the client isn't connected.
			 */
			DictSignal broadcastCurrentPos() const;

			/** Request the playlist loaded broadcast from the server.
			 *
			 *  Everytime someone loads a saved playlist this will be emitted.
			 *
			 *  @param slot Function pointer to a function taking a
			 *              const string& and returning a bool.
			 *  @param error Function pointer to an error callback
			 *               function. (<b>optional</b>)
			 *
			 *  @throw connection_error If the client isn't connected.
			 */
			StringSignal broadcastLoaded() const;

		/** @cond */
		private:
			friend class Client;
			Playlist( xmmsc_connection_t*& conn, bool& connected,
			          MainloopInterface*& ml );

			Playlist( const Playlist& src );
			Playlist& operator=( const Playlist& src );

			xmmsc_connection_t*& conn_;
			bool& connected_;
			MainloopInterface*& ml_;
		/** @endcond */

	};

}

#endif // XMMSCLIENTPP_PLAYLIST_HH
