#ifndef XMMSCLIENTPP_PLAYLIST_HH
#define XMMSCLIENTPP_PLAYLIST_HH

#include <xmmsclient/xmmsclient.h>
#include <xmmsclient/xmmsclient++/list.h>
#include <xmmsclient/xmmsclient++/mainloop.h>
#include <xmmsclient/xmmsclient++/coll.h>
#include <xmmsclient/xmmsclient++/signal.h>
#include <xmmsclient/xmmsclient++/helpers.h>
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
			void list() const;

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
			void load( const std::string& playlist ) const;

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
			void addUrl( const std::string& url,
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
			void addUrl( const std::string& url,
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
			void addUrlEncoded( const std::string& url,
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
			void addRecursive( const std::string& url,
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
			void addRecursiveEncoded( const std::string& url,
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
			void addId( const unsigned int id,
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
			void addCollection( const Coll::Coll& collection,
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
			void clear( const std::string& playlist = DEFAULT_PLAYLIST ) const;

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
			 *  @return current position as unsigned integer.
			 */
			unsigned int currentPos( const std::string& playlist = DEFAULT_PLAYLIST
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
			std::string currentActive() const;

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
			void insertUrl( int pos, const std::string& url,
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
			void insertUrl( int pos, const std::string& url,
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
			void insertUrlEncoded( int pos, const std::string& url,
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
			void insertId( int pos, unsigned int id,
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
			void insertCollection( int pos, const Coll::Coll& collection,
			                       const std::list< std::string >& order,
			                       const std::string& playlist = DEFAULT_PLAYLIST
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
			List< unsigned int > listEntries( const std::string& playlist
			                                                = DEFAULT_PLAYLIST
			                                ) const;

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
			void moveEntry( unsigned int curpos, unsigned int newpos,
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
			void removeEntry( unsigned int pos,
			                  const std::string& playlist = DEFAULT_PLAYLIST
			                ) const;

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
			unsigned int setNext( unsigned int pos ) const;

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
			unsigned int setNextRel( signed int pos ) const;

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
			void shuffle( const std::string& playlist = DEFAULT_PLAYLIST
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
			void sort( const std::list<std::string>& properties,
			           const std::string& playlist = DEFAULT_PLAYLIST ) const;

			/** Get the list of saved playlists.
			 *
			 *  @param slot Function pointer to a function returning a bool.
			 *  @param error Function pointer to an error callback
			 *               function. (<b>optional</b>)
			 *
			 *  @throw connection_error If the client isn't connected.
			 */
			void
			list( const VoidSlot& slot,
			      const ErrorSlot& error = &Xmms::dummy_error ) const;

			/** Load a saved playlist and make it the active playlist.
			 *
			 *  @param playlist the playlist to load.
			 *  @param slot Function pointer to a function returning a bool.
			 *  @param error Function pointer to an error callback
			 *               function. (<b>optional</b>)
			 *
			 *  @throw connection_error If the client isn't connected.
			 */
			void
			load( const std::string& playlist,
			      const VoidSlot& slot,
			      const ErrorSlot& error = &Xmms::dummy_error ) const;

			/**	Add the url to a playlist.
			 *  The url should be absolute to the server-side. 
			 *  Note that you will have to include the protocol 
			 *  for the url to. ie: file://mp3/my_mp3s/first.mp3.
			 *  
			 *  @param url file to be added
			 *  @param playlist the playlist to modify (if omitted,
			 *                  act on the current playlist)
			 *  @param slot Function pointer to a function returning a bool.
			 *  @param error Function pointer to an error callback
			 *               function. (<b>optional</b>)
			 *
			 *  @throw connection_error If the client isn't connected.
			 */
			void
			addUrl( const std::string& url,
			        const std::string& playlist,
			        const VoidSlot& slot,
			        const ErrorSlot& error = &Xmms::dummy_error ) const;
			/**
			 * @overload
			 * @note Acts on the current playlist.
			 */
			void
			addUrl( const std::string& url,
			        const VoidSlot& slot,
			        const ErrorSlot& error = &Xmms::dummy_error ) const;

			/** Add the url with args to a playlist.
			 *  The url should be absolute to the server-side.
			 *  Note that you will have to include the protocol
			 *  for the url to. ie: file://mp3/my_mp3s/first.mp3.
			 *
			 *  @param url file to be added
			 *  @param args List of strings used as arguments.
			 *  @param playlist the playlist to modify (if omitted,
			 *                  act on the current playlist)
			 *  @param slot Function pointer to a function returning a bool.
			 *  @param error Function pointer to an error callback
			 *               function. (<b>optional</b>)
			 *
			 *  @throw connection_error If the client isn't connected.
			 */
			void
			addUrl( const std::string& url,
			        const std::list< std::string >& args,
			        const std::string& playlist,
			        const VoidSlot& slot,
			        const ErrorSlot& error = &Xmms::dummy_error ) const;
			/**
			 * @overload
			 * @note Acts on the current playlist.
			 */
			void
			addUrl( const std::string& url,
			        const std::list< std::string >& args,
			        const VoidSlot& slot,
			        const ErrorSlot& error = &Xmms::dummy_error ) const;

			/**	Add the directory recursivly to a playlist.
			 *  The url should be absolute to the server-side. 
			 *  Note that you will have to include the protocol 
			 *  for the url to. ie: file://mp3/my_mp3s/directory
			 *  
			 *  @param url directory to be added
			 *  @param playlist the playlist to modify (if omitted,
			 *                  act on the current playlist)
			 *  @param slot Function pointer to a function returning a bool.
			 *  @param error Function pointer to an error callback
			 *               function. (<b>optional</b>)
			 *
			 *  @throw connection_error If the client isn't connected.
			 */
			void
			addRecursive( const std::string& url,
			              const std::string& playlist,
			              const VoidSlot& slot,
			              const ErrorSlot& error = &Xmms::dummy_error ) const;

			/**
			 * @overload
			 * @note Acts on the current playlist.
			 */
			void
			addRecursive( const std::string& url,
			              const VoidSlot& slot,
			              const ErrorSlot& error = &Xmms::dummy_error ) const;

			/**	Add the url to a playlist.
			 *  Same as #addUrl but takes an Encoded Url instead.
			 *  
			 *  @param url file to be added
			 *  @param playlist the playlist to modify (if omitted,
			 *                  act on the current playlist)
			 *  @param slot Function pointer to a function returning a bool.
			 *  @param error Function pointer to an error callback
			 *               function. (<b>optional</b>)
			 *
			 *  @throw connection_error If the client isn't connected.
			 */
			void
			addUrlEncoded( const std::string& url,
			               const std::string& playlist,
			               const VoidSlot& slot,
			               const ErrorSlot& error = &Xmms::dummy_error ) const;

			/**
			 * @overload
			 * @note Acts on the current playlist.
			 */
			void
			addUrlEncoded( const std::string& url,
			               const VoidSlot& slot,
			               const ErrorSlot& error = &Xmms::dummy_error ) const;

			/**	Add the directory recursivly to a playlist.
			 *  Same as #addRecursive but takes a Encoded URL instead
			 *  
			 *  @param url directory to be added
			 *  @param playlist the playlist to modify (if omitted,
			 *                  act on the current playlist)
			 *  @param slot Function pointer to a function returning a bool.
			 *  @param error Function pointer to an error callback
			 *               function. (<b>optional</b>)
			 *
			 *  @throw connection_error If the client isn't connected.
			 */
			void
			addRecursiveEncoded( const std::string& url,
			                     const std::string& playlist,
			                     const VoidSlot& slot,
			                     const ErrorSlot& error = &Xmms::dummy_error ) const;

			/**
			 * @overload
			 * @note Acts on the current playlist.
			 */
			void
			addRecursiveEncoded( const std::string& url,
			                     const VoidSlot& slot,
			                     const ErrorSlot& error = &Xmms::dummy_error ) const;


			/** Add a medialib id to a playlist.
			 *
			 *  @param id A medialib ID
			 *  @param playlist the playlist to modify (if omitted,
			 *                  act on the current playlist)
			 *  @param slot Function pointer to a function returning a bool.
			 *  @param error Function pointer to an error callback
			 *               function. (<b>optional</b>)
			 *
			 *  @throw connection_error If the client isn't connected.
			 */
			void
			addId( const unsigned int id,
			       const std::string& playlist,
			       const VoidSlot& slot,
			       const ErrorSlot& error = &Xmms::dummy_error ) const;

			/**
			 * @overload
			 * @note Acts on the current playlist.
			 */
			void
			addId( const unsigned int id,
			       const VoidSlot& slot,
			       const ErrorSlot& error = &Xmms::dummy_error ) const;

			/** Add the content of the given collection to a playlist.
			 *  The list of ordering properties defines how the set of
			 *  matching media is added.
			 *
			 *  @param collection The collection from which media will
			 *                    be added to the playlist.
			 *  @param order The order in which the matched media are added.
			 *  @param playlist the playlist to modify (if omitted,
			 *                  act on the current playlist)
			 *  @param slot Function pointer to a function returning a bool.
			 *  @param error Function pointer to an error callback
			 *               function. (<b>optional</b>)
			 *
			 *  @throw connection_error If the client isn't connected.
			 */
			void
			addCollection( const Coll::Coll& collection,
			               const std::list< std::string >& order,
			               const std::string& playlist,
			               const VoidSlot& slot,
			               const ErrorSlot& error = &Xmms::dummy_error ) const;

			/**
			 * @overload
			 * @note Acts on the current playlist.
			 */
			void
			addCollection( const Coll::Coll& collection,
			               const std::list< std::string >& order,
			               const VoidSlot& slot,
			               const ErrorSlot& error = &Xmms::dummy_error ) const;

			/** Clears a playlist.
			 * 
			 *  @param playlist the playlist to modify (if omitted,
			 *                  act on the current playlist)
			 *  @param slot Function pointer to a function returning a bool.
			 *  @param error Function pointer to an error callback
			 *               function. (<b>optional</b>)
			 *
			 *  @throw connection_error If the client isn't connected.
			 */
			void 
			clear( const std::string& playlist,
			       const VoidSlot& slot,
			       const ErrorSlot& error = &Xmms::dummy_error ) const;

			/**
			 * @overload
			 * @note Acts on the current playlist.
			 */
			void 
			clear( const VoidSlot& slot,
			       const ErrorSlot& error = &Xmms::dummy_error ) const;

			/** Retrieve the current position in a playlist.
			 * 
			 *  @param playlist the playlist to consider (if omitted,
			 *                  act on the current playlist)
			 *  @param slot Function pointer to a function taking a
			 *              const unsigned int& and returning a bool.
			 *  @param error Function pointer to an error callback
			 *               function. (<b>optional</b>)
			 *
			 *  @throw connection_error If the client isn't connected.
			 */
			void
			currentPos( const std::string& playlist,
			            const UintSlot& slot,
			            const ErrorSlot& error = &Xmms::dummy_error ) const;

			/**
			 * @overload
			 * @note Acts on the current playlist.
			 */
			void
			currentPos( const UintSlot& slot,
			            const ErrorSlot& error = &Xmms::dummy_error ) const;

			/** Retrieve the name of the current active playlist.
			 *
			 *  @param slot Function pointer to a function taking a
			 *              const string& and returning a bool.
			 *  @param error Function pointer to an error callback
			 *               function. (<b>optional</b>)
			 *
			 *  @throw connection_error If the client isn't connected.
			 */
			void
			currentActive( const StringSlot& slot,
			               const ErrorSlot& error = &Xmms::dummy_error ) const;

			/** Insert an entry at given position in a playlist.
			 *  
			 *  @param pos A position in the playlist.
			 *  @param url The URL to insert.
			 *  @param playlist the playlist to modify (if omitted,
			 *                  act on the current playlist)
			 *  @param slot Function pointer to a function returning a bool.
			 *  @param error Function pointer to an error callback
			 *               function. (<b>optional</b>)
			 *
			 *  @throw connection_error If the client isn't connected.
			 */
			void
			insertUrl( int pos, const std::string& url,
			           const std::string& playlist,
			           const VoidSlot& slot,
					   const ErrorSlot& error = &Xmms::dummy_error ) const;

			/**
			 * @overload
			 * @note Acts on the current playlist.
			 */
			void
			insertUrl( int pos, const std::string& url,
			           const VoidSlot& slot,
					   const ErrorSlot& error = &Xmms::dummy_error ) const;

			/** Insert an entry with args at given position in a playlist.
			 *
			 *  @param pos A position in the playlist.
			 *  @param url The URL to insert.
			 *  @param args List of strings used as arguments.
			 *  @param playlist the playlist to modify (if omitted,
			 *                  act on the current playlist)
			 *  @param slot Function pointer to a function returning a bool.
			 *  @param error Function pointer to an error callback
			 *               function. (<b>optional</b>)
			 *
			 *  @throw connection_error If the client isn't connected.
			 */
			void
			insertUrl( int pos, const std::string& url,
			           const std::list< std::string >& args,
			           const std::string& playlist,
			           const VoidSlot& slot,
					   const ErrorSlot& error = &Xmms::dummy_error ) const;

			/**
			 * @overload
			 * @note Acts on the current playlist.
			 */
			void
			insertUrl( int pos, const std::string& url,
			           const std::list< std::string >& args,
			           const VoidSlot& slot,
					   const ErrorSlot& error = &Xmms::dummy_error ) const;

			/** Insert an entry at given position in a playlist.
			 *  same as #insertUrl but takes a encoded url instead.
			 *  
			 *  @param pos A position in the playlist.
			 *  @param url The URL to insert.
			 *  @param playlist the playlist to modify (if omitted,
			 *                  act on the current playlist)
			 *  @param slot Function pointer to a function returning a bool.
			 *  @param error Function pointer to an error callback
			 *               function. (<b>optional</b>)
			 *
			 *  @throw connection_error If the client isn't connected.
			 */
			void
			insertUrlEncoded( int pos, const std::string& url,
			                  const std::string& playlist,
			                  const VoidSlot& slot,
					          const ErrorSlot& error = &Xmms::dummy_error ) const;

			/**
			 * @overload
			 * @note Acts on the current playlist.
			 */
			void
			insertUrlEncoded( int pos, const std::string& url,
			                  const VoidSlot& slot,
					          const ErrorSlot& error = &Xmms::dummy_error ) const;

			/** Insert a medialib ID at given position in a playlist.
			 *
			 *  @param pos A position in the playlist.
			 *  @param id A medialib ID.
			 *  @param playlist the playlist to modify (if omitted,
			 *                  act on the current playlist)
			 *  @param slot Function pointer to a function returning a bool.
			 *  @param error Function pointer to an error callback
			 *               function. (<b>optional</b>)
			 *
			 *  @throw connection_error If the client isn't connected.
			 */
			void
			insertId( int pos, unsigned int id,
			          const std::string& playlist,
			          const VoidSlot& slot,
			          const ErrorSlot& error = &Xmms::dummy_error ) const;

			/**
			 * @overload
			 * @note Acts on the current playlist.
			 */
			void
			insertId( int pos, unsigned int id,
			          const VoidSlot& slot,
			          const ErrorSlot& error = &Xmms::dummy_error ) const;

			/** Insert the content of the given collection at given
			 *  position in a playlist.
			 *  The list of ordering properties defines how the set of
			 *  matching media is added.
			 *
			 *  @param pos A position in the playlist.
			 *  @param collection The collection from which media will
			 *                    be inserted into the playlist.
			 *  @param order The order in which the matched media are inserted.
			 *  @param playlist the playlist to modify (if omitted,
			 *                  act on the current playlist)
			 *  @param slot Function pointer to a function returning a bool.
			 *  @param error Function pointer to an error callback
			 *               function. (<b>optional</b>)
			 *
			 *  @throw connection_error If the client isn't connected.
			 */
			void
			insertCollection( int pos, const Coll::Coll& collection,
			                  const std::list< std::string >& order,
			                  const std::string& playlist,
			                  const VoidSlot& slot,
			                  const ErrorSlot& error = &Xmms::dummy_error
			                ) const;

			/**
			 * @overload
			 * @note Acts on the current playlist.
			 */
			void
			insertCollection( int pos, const Coll::Coll& collection,
			                  const std::list< std::string >& order,
			                  const VoidSlot& slot,
			                  const ErrorSlot& error = &Xmms::dummy_error
			                ) const;

			/** Retrieve the entries of a playlist.
			 *
			 *  @param playlist the playlist to consider (if omitted,
			 *                  act on the current playlist)
			 *  @param slot Function pointer to a function taking a
			 *              const List< unsigned int >& and returning a bool.
			 *  @param error Function pointer to an error callback
			 *               function. (<b>optional</b>)
			 *
			 *  @throw connection_error If the client isn't connected.
			 */
			void
			listEntries( const std::string& playlist,
			             const UintListSlot& slot,
			             const ErrorSlot& error = &Xmms::dummy_error ) const;

			/**
			 * @overload
			 * @note Acts on the current playlist.
			 */
			void
			listEntries( const UintListSlot& slot,
			             const ErrorSlot& error = &Xmms::dummy_error ) const;

			/** Move a playlist entry to a new position (absolute move).
			 * 
			 *  @param curpos Position of the entry to be moved.
			 *  @param newpos Position where the entry should be moved to.
			 *  @param playlist the playlist to modify (if omitted,
			 *                  act on the current playlist)
			 *  @param slot Function pointer to a function returning a bool.
			 *  @param error Function pointer to an error callback
			 *               function. (<b>optional</b>)
			 *
			 *  @throw connection_error If the client isn't connected.
			 */
			void
			moveEntry( unsigned int curpos, unsigned int newpos,
			           const std::string& playlist,
			           const VoidSlot& slot,
			           const ErrorSlot& error = &Xmms::dummy_error ) const;

			/**
			 * @overload
			 * @note Acts on the current playlist.
			 */
			void
			moveEntry( unsigned int curpos, unsigned int newpos,
			           const VoidSlot& slot,
			           const ErrorSlot& error = &Xmms::dummy_error ) const;

			/** Remove an entry from a playlist.
			 * 
			 *  @param pos The position that should be 
			 *             removed from the playlist.
			 *  @param playlist the playlist to modify (if omitted,
			 *                  act on the current playlist)
			 *  @param slot Function pointer to a function returning a bool.
			 *  @param error Function pointer to an error callback
			 *               function. (<b>optional</b>)
			 *
			 *  @throw connection_error If the client isn't connected.
			 */
			void
			removeEntry( unsigned int pos,
			             const std::string& playlist,
			             const VoidSlot& slot,
					     const ErrorSlot& error = &Xmms::dummy_error ) const;

			/**
			 * @overload
			 * @note Acts on the current playlist.
			 */
			void
			removeEntry( unsigned int pos,
			             const VoidSlot& slot,
					     const ErrorSlot& error = &Xmms::dummy_error ) const;

			/** Set next entry in the playlist.
			 * 
			 *  @param pos A position to jump to.
			 *  @param slot Function pointer to a function taking a
			 *              const unsigned int& and returning a bool.
			 *  @param error Function pointer to an error callback
			 *               function. (<b>optional</b>)
			 *
			 *  @throw connection_error If the client isn't connected.
			 */
			void
			setNext( unsigned int pos,
			         const UintSlot& slot,
					 const ErrorSlot& error = &Xmms::dummy_error ) const;

			/** Same as setNext but relative to the current position.
			 *
			 *  -1 will back one and 1 will move to the next entry.
			 *
			 *  @param pos Relative position for the jump.
			 *  @param slot Function pointer to a function taking a
			 *              const unsigned int& and returning a bool.
			 *  @param error Function pointer to an error callback
			 *               function. (<b>optional</b>)
			 *
			 *  @throw connection_error If the client isn't connected.
			 */
			void
			setNextRel( signed int pos,
			            const UintSlot& slot,
			            const ErrorSlot& error = &Xmms::dummy_error ) const;

			/** Shuffles a playlist.
			 *
			 *  @param playlist the playlist to modify (if omitted,
			 *                  act on the current playlist)
			 *  @param slot Function pointer to a function returning a bool.
			 *  @param error Function pointer to an error callback
			 *               function. (<b>optional</b>)
			 *
			 *  @throw connection_error If the client isn't connected.
			 */
			void
			shuffle( const std::string& playlist,
			         const VoidSlot& slot,
			         const ErrorSlot& error = &Xmms::dummy_error ) const;

			/**
			 * @overload
			 * @note Acts on the current playlist.
			 */
			void
			shuffle( const VoidSlot& slot,
			         const ErrorSlot& error = &Xmms::dummy_error ) const;

			/** Sorts a playlist according to a list of properties.
			 * 
			 *  @param properties Properties to sort by.
			 *  @param playlist the playlist to modify (if omitted,
			 *                  act on the current playlist)
			 *  @param slot Function pointer to a function returning a bool.
			 *  @param error Function pointer to an error callback
			 *               function. (<b>optional</b>)
			 *
			 *  @throw connection_error If the client isn't connected.
			 */
			void
			sort( const std::list<std::string>& property,
			      const std::string& playlist,
			      const VoidSlot& slot,
			      const ErrorSlot& error = &Xmms::dummy_error ) const;

			/**
			 * @overload
			 * @note Acts on the current playlist.
			 */
			void
			sort( const std::list<std::string>& property,
			      const VoidSlot& slot,
			      const ErrorSlot& error = &Xmms::dummy_error ) const;

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
			void
			broadcastChanged( const DictSlot& slot,
			                  const ErrorSlot& error = &Xmms::dummy_error
			                ) const; 

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
			void
			broadcastCurrentPos( const UintSlot& slot,
			                     const ErrorSlot& error = &Xmms::dummy_error
			                   ) const;

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
			void
			broadcastLoaded( const StringSlot& slot,
			                 const ErrorSlot& error = &Xmms::dummy_error
			               ) const;

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
