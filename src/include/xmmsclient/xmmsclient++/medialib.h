#ifndef XMMSCLIENTPP_MEDIALIB_H
#define XMMSCLIENTPP_MEDIALIB_H

#include <xmmsclient/xmmsclient.h>
#include <xmmsclient/xmmsclient++/mainloop.h>
#include <xmmsclient/xmmsclient++/dict.h>
#include <xmmsclient/xmmsclient++/list.h>
#include <xmmsclient/xmmsclient++/typedefs.h>
#include <xmmsclient/xmmsclient++/helpers.h>

#include <string>

namespace Xmms
{

	class Client;

	/** @class Medialib medialib.h "xmmsclient/xmmsclient++/medialib.h"
	 *  @brief This class controls the medialib.
	 */
	class Medialib
	{

		public:

			/** Destructor. */
			~Medialib();

			/** Escape a string so that it can be used in sqlite queries. 
			 *
			 *  @param input The input string to prepare.
			 *
			 *  @return The prepared version of the input string.
			 */
			std::string sqlitePrepareString( const std::string& input ) const;

			/** Add a URL to the medialib.
			 *  If you want to add multiple files you should call pathImport.
			 *
			 *  @param url URL to add to the medialib.
			 *
			 *  @throw connection_error If the client isn't connected.
			 *  @throw mainloop_running_error If a mainloop is running -
			 *  sync functions can't be called when mainloop is running. This
			 *  is only thrown if the programmer is careless or doesn't know
			 *  what he/she's doing. (logic_error)
			 *  @throw result_error If the operation failed.
			 */
			void addEntry( const std::string& url ) const;

			/** Queries the medialib for files and adds the matching ones
			 *  to the current playlist.
			 *  Remember to include a field called id in the query.
			 *
			 *  @param query SQL-query to medialib.
			 *
			 *  @throw connection_error If the client isn't connected.
			 *  @throw mainloop_running_error If a mainloop is running -
			 *  sync functions can't be called when mainloop is running. This
			 *  is only thrown if the programmer is careless or doesn't know
			 *  what he/she's doing. (logic_error)
			 *  @throw result_error If the operation failed.
			 */
			void addToPlaylist( const std::string& query ) const;

			/** Remove a custom field in the medialib associated with an entry.
			 *
			 *  The optional @c source parameter will default to
			 *  client/@<clientname@> if not provided.
			 *
			 *  @param id Entry ID.
			 *  @param key Field key to remove.
			 *  @param source Source for the entry. (<b>optional</b>)
			 *
			 *  @throw connection_error If the client isn't connected.
			 *  @throw mainloop_running_error If a mainloop is running -
			 *  sync functions can't be called when mainloop is running. This
			 *  is only thrown if the programmer is careless or doesn't know
			 *  what he/she's doing. (logic_error)
			 *  @throw result_error If the operation failed.
			 */
			void entryPropertyRemove( unsigned int id,
			                          const std::string& key,
			                          const std::string& source = "" ) const;

			/** Associate a value with a medialib entry.
			 *
			 *  The optional @c source parameter will default to
			 *  client/@<clientname@> if not provided.
			 *
			 *  @param id Entry ID.
			 *  @param key Field key to set.
			 *  @param value to set.
			 *  @param source Source for the value. (<b>optional</b>)
			 *
			 *  @throw connection_error If the client isn't connected.
			 *  @throw mainloop_running_error If a mainloop is running -
			 *  sync functions can't be called when mainloop is running. This
			 *  is only thrown if the programmer is careless or doesn't know
			 *  what he/she's doing. (logic_error)
			 *  @throw result_error If the operation failed.
			 */
			void entryPropertySet( unsigned int id,
			                       const std::string& key,
			                       const std::string& value,
			                       const std::string& source = "" ) const;

			/** Search for a entry (URL) in the medialib db
			 *  and return its ID number.
			 *
			 *  @param url The URL to search for.
			 *
			 *  @throw connection_error If the client isn't connected.
			 *  @throw mainloop_running_error If a mainloop is running -
			 *  sync functions can't be called when mainloop is running. This
			 *  is only thrown if the programmer is careless or doesn't know
			 *  what he/she's doing. (logic_error)
			 *  @throw result_error If the operation failed.
			 *  
			 *  @return ID number
			 */
			unsigned int getID( const std::string& url ) const;

			/** Retrieve information about a entry from the medialib.
			 *
			 *  @param id ID of the entry.
			 *
			 *  @throw connection_error If the client isn't connected.
			 *  @throw mainloop_running_error If a mainloop is running -
			 *  sync functions can't be called when mainloop is running. This
			 *  is only thrown if the programmer is careless or doesn't know
			 *  what he/she's doing. (logic_error)
			 *  @throw result_error If the operation failed.
			 */  
			PropDict getInfo( unsigned int id ) const;

			/** Import all files recursively from the 
			 *  directory passed as argument.
			 *
			 *  @param path Directory to import.
			 *
			 *  @throw connection_error If the client isn't connected.
			 *  @throw mainloop_running_error If a mainloop is running -
			 *  sync functions can't be called when mainloop is running. This
			 *  is only thrown if the programmer is careless or doesn't know
			 *  what he/she's doing. (logic_error)
			 *  @throw result_error If the operation failed.
			 */
			void pathImport( const std::string& path ) const;

			/** Export a serverside playlist to a format 
			 *  that could be read from another mediaplayer.
			 *
			 *  @param playlist Name of the serverside playlist.
			 *  @param mime Mimetype of the export format.
			 *
			 *  @throw connection_error If the client isn't connected.
			 *  @throw mainloop_running_error If a mainloop is running -
			 *  sync functions can't be called when mainloop is running. This
			 *  is only thrown if the programmer is careless or doesn't know
			 *  what he/she's doing. (logic_error)
			 *  @throw result_error If the operation failed.
			 *  
			 *  @return The playlist as string.
			 *
			 *  @todo copying this can be quite expensive, should probably
			 *  copy a shared_ptr instead... ?
			 */
			const std::string playlistExport( const std::string& playlist,
			                                  const std::string& mime )  const;

			/** Import a playlist from a playlist file.
			 *
			 *  @param playlist The name of the new playlist.
			 *  @param url URL to the playlist file.
			 *
			 *  @throw connection_error If the client isn't connected.
			 *  @throw mainloop_running_error If a mainloop is running -
			 *  sync functions can't be called when mainloop is running. This
			 *  is only thrown if the programmer is careless or doesn't know
			 *  what he/she's doing. (logic_error)
			 *  @throw result_error If the operation failed.
			 */
			void playlistImport( const std::string& playlist,
			                     const std::string& url ) const;
			
			/** Get a playlist.
			 *
			 *  @param playlist Name of the playlist.
			 *
			 *  @throw connection_error If the client isn't connected.
			 *  @throw mainloop_running_error If a mainloop is running -
			 *  sync functions can't be called when mainloop is running. This
			 *  is only thrown if the programmer is careless or doesn't know
			 *  what he/she's doing. (logic_error)
			 *  @throw result_error If the operation failed.
			 *
			 *  @return List of unsigned ints representing medialib ID.
			 */
			List< unsigned int > 
			playlistList( const std::string& playlist ) const;

			/** Load a playlist from the medialib 
			 *  to the current active playlist.
			 *
			 *  @param name Name of the playlist.
			 *
			 *  @throw connection_error If the client isn't connected.
			 *  @throw mainloop_running_error If a mainloop is running -
			 *  sync functions can't be called when mainloop is running. This
			 *  is only thrown if the programmer is careless or doesn't know
			 *  what he/she's doing. (logic_error)
			 *  @throw result_error If the operation failed.
			 */
			void playlistLoad( const std::string& name ) const;

			/** Remove a playlist from the medialib, 
			 *  keeping the songs of course.
			 *
			 *  @param playlist The playlist to remove.
			 *
			 *  @throw connection_error If the client isn't connected.
			 *  @throw mainloop_running_error If a mainloop is running -
			 *  sync functions can't be called when mainloop is running. This
			 *  is only thrown if the programmer is careless or doesn't know
			 *  what he/she's doing. (logic_error)
			 *  @throw result_error If the operation failed.
			 */
			void playlistRemove( const std::string& playlist ) const;

			/** Save the current playlist to a serverside playlist.
			 *
			 *  @param name Name to save the playlist as.
			 *
			 *  @throw connection_error If the client isn't connected.
			 *  @throw mainloop_running_error If a mainloop is running -
			 *  sync functions can't be called when mainloop is running. This
			 *  is only thrown if the programmer is careless or doesn't know
			 *  what he/she's doing. (logic_error)
			 *  @throw result_error If the operation failed.
			 */
			void playlistSaveCurrent( const std::string& name ) const;

			/** Returns a list of all available playlists.
			 *
			 *  @throw connection_error If the client isn't connected.
			 *  @throw mainloop_running_error If a mainloop is running -
			 *  sync functions can't be called when mainloop is running. This
			 *  is only thrown if the programmer is careless or doesn't know
			 *  what he/she's doing. (logic_error)
			 *  @throw result_error If the operation failed.
			 *
			 *  @return List of strings with playlist names.
			 */
			List< std::string > playlistsList() const;

			/** Rehash the medialib. 
			 *  This will check data in the medialib still 
			 *  is the same as the data in files.
			 *
			 *  @param id The ID to rehash. (<b>optional</b>, 
			 *                               default is to rehash all)
			 *
			 *  @throw connection_error If the client isn't connected.
			 *  @throw mainloop_running_error If a mainloop is running -
			 *  sync functions can't be called when mainloop is running. This
			 *  is only thrown if the programmer is careless or doesn't know
			 *  what he/she's doing. (logic_error)
			 *  @throw result_error If the operation failed.
			 */
			void rehash( unsigned int id = 0 ) const;

			/** Remove an entry from the medialib.
			 *  
			 *  @param id ID of the entry to remove.
			 *
			 *  @throw connection_error If the client isn't connected.
			 *  @throw mainloop_running_error If a mainloop is running -
			 *  sync functions can't be called when mainloop is running. This
			 *  is only thrown if the programmer is careless or doesn't know
			 *  what he/she's doing. (logic_error)
			 *  @throw result_error If the operation failed.
			 */
			void removeEntry( unsigned int id ) const;

			/** Make a SQL query to the server medialib.
			 *  
			 *  @param query SQL query.
			 *
			 *  @throw connection_error If the client isn't connected.
			 *  @throw mainloop_running_error If a mainloop is running -
			 *  sync functions can't be called when mainloop is running. This
			 *  is only thrown if the programmer is careless or doesn't know
			 *  what he/she's doing. (logic_error)
			 *  @throw result_error If the operation failed.
			 *
			 *  @return List of @link Dict Dicts@endlink
			 */
			List< Dict > select( const std::string& query ) const;

			/** Add a URL to the medialib.
			 *  If you want to add multiple files you should call pathImport.
			 *
			 *  @param url URL to add to the medialib.
			 *  @param slot Function pointer to a function returning a bool.
			 *  @param error Function pointer to an error callback
			 *               function. (<b>optional</b>)
			 *
			 *  @throw connection_error If the client isn't connected.
			 */
			void
			addEntry( const std::string& url,
			          const VoidSlot& slot,
			          const ErrorSlot& error = &Xmms::dummy_error ) const;

			/**
			 * @overload
			 * @note It takes a list of slots instead of just one slot.
			 */
			void
			addEntry( const std::string& url,
			          const std::list< VoidSlot >& slots,
			          const ErrorSlot& error = &Xmms::dummy_error ) const;

			/** Queries the medialib for files and adds the matching ones
			 *  to the current playlist.
			 *  Remember to include a field called id in the query.
			 *
			 *  @param query SQL-query to medialib.
			 *  @param slot Function pointer to a function returning a bool.
			 *  @param error Function pointer to an error callback
			 *               function. (<b>optional</b>)
			 *
			 *  @throw connection_error If the client isn't connected.
			 */
			void
			addToPlaylist( const std::string& query,
			               const VoidSlot& slot,
			               const ErrorSlot& error = &Xmms::dummy_error ) const;

			/**
			 * @overload
			 * @note It takes a list of slots instead of just one slot.
			 */
			void
			addToPlaylist( const std::string& query,
			               const std::list< VoidSlot >& slots,
			               const ErrorSlot& error = &Xmms::dummy_error ) const;

			/** Remove a custom field in the medialib associated with an entry.
			 *
			 *  @note This function is overloaded to provide one which takes
			 *  @c source and one which doesn't, if the one which doesn't
			 *  is used, the it will default to client/@<clientname@>.
			 *
			 *  @param id Entry ID.
			 *  @param key Field key to remove.
			 *  @param source Source for the entry.
			 *  @param slot Function pointer to a function returning a bool.
			 *  @param error Function pointer to an error callback
			 *               function. (<b>optional</b>)
			 *
			 *  @throw connection_error If the client isn't connected.
			 */
			void
			entryPropertyRemove( unsigned int id, const std::string& key,
			                     const std::string& source,
			                     const VoidSlot& slot,
			                     const ErrorSlot& error = &Xmms::dummy_error
			                   ) const;
			
			/**
			 * @overload
			 * @note It takes a list of slots instead of just one slot.
			 */
			void
			entryPropertyRemove( unsigned int id, const std::string& key,
			                     const std::string& source,
			                     const std::list< VoidSlot >& slots,
			                     const ErrorSlot& error = &Xmms::dummy_error
			                   ) const;
			
			/**
			 * @overload
			 * @note It doesn't take the source.
			 */
			void
			entryPropertyRemove( unsigned int id, const std::string& key,
			                     const VoidSlot& slot,
			                     const ErrorSlot& error = &Xmms::dummy_error
			                   ) const;

			/**
			 * @overload
			 * @note It doesn't take the source.
			 * @note It takes a list of slots instead of just one slot.
			 */
			void
			entryPropertyRemove( unsigned int id, const std::string& key,
			                     const std::list< VoidSlot >& slots,
			                     const ErrorSlot& error = &Xmms::dummy_error
			                   ) const;
			
			/** Associate a value with a medialib entry.
			 *
			 *  @note This function is overloaded to provide one which takes
			 *  @c source and one which doesn't, if the one which doesn't
			 *  is used, the it will default to client/@<clientname@>.
			 *
			 *  @param id Entry ID.
			 *  @param key Field key to set.
			 *  @param value to set.
			 *  @param source Source for the value.
			 *  @param slot Function pointer to a function returning a bool.
			 *  @param error Function pointer to an error callback
			 *               function. (<b>optional</b>)
			 *
			 *  @throw connection_error If the client isn't connected.
			 */
			void
			entryPropertySet( unsigned int id, const std::string& key,
			                  const std::string& value,
			                  const std::string& source,
			                  const VoidSlot& slot,
			                  const ErrorSlot& error = &Xmms::dummy_error
			                ) const;

			/**
			 * @overload
			 * @note It takes a list of slots instead of just one slot.
			 */
			void
			entryPropertySet( unsigned int id, const std::string& key,
			                  const std::string& value,
			                  const std::string& source,
			                  const std::list< VoidSlot >& slots,
			                  const ErrorSlot& error = &Xmms::dummy_error
			                ) const;

			/**
			 * @overload
			 * @note It doesn't take the source.
			 */
			void
			entryPropertySet( unsigned int id, const std::string& key,
			                  const std::string& value, const VoidSlot& slot,
			                  const ErrorSlot& error = &Xmms::dummy_error
			                ) const;
			
			/**
			 * @overload
			 * @note It doesn't take the source.
			 * @note It takes a list of slots instead of just one slot.
			 */
			void
			entryPropertySet( unsigned int id, const std::string& key,
			                  const std::string& value,
			                  const std::list< VoidSlot >& slots,
			                  const ErrorSlot& error = &Xmms::dummy_error
			                ) const;

			/** Search for a entry (URL) in the medialib db
			 *  and return its ID number.
			 *
			 *  @param url The URL to search for.
			 *  @param slot Function pointer to a function taking a
			 *              const unsigned int& and returning a bool.
			 *  @param error Function pointer to an error callback
			 *               function. (<b>optional</b>)
			 *
			 *  @throw connection_error If the client isn't connected.
			 */
			void
			getID( const std::string& url, const UintSlot& slot,
			       const ErrorSlot& error = &Xmms::dummy_error ) const;

			/**
			 * @overload
			 * @note It takes a list of slots instead of just one slot.
			 */
			void
			getID( const std::string& url, const std::list< UintSlot >& slots,
			       const ErrorSlot& error = &Xmms::dummy_error ) const;

			/** Retrieve information about a entry from the medialib.
			 *
			 *  @param id ID of the entry.
			 *  @param slot Function pointer to a function taking a
			 *              const PropDict& and returning a bool.
			 *  @param error Function pointer to an error callback
			 *               function. (<b>optional</b>)
			 *
			 *  @throw connection_error If the client isn't connected.
			 */
			void
			getInfo( unsigned int id, const PropDictSlot& slot,
			         const ErrorSlot& error = &Xmms::dummy_error ) const;

			/**
			 * @overload
			 * @note It takes a list of slots instead of just one slot.
			 */
			void
			getInfo( unsigned int id, const std::list< PropDictSlot >& slots,
			         const ErrorSlot& error = &Xmms::dummy_error ) const;

			/** Import all files recursively from the 
			 *  directory passed as argument.
			 *
			 *  @param path Directory to import.
			 *  @param slot Function pointer to a function returning a bool.
			 *  @param error Function pointer to an error callback
			 *               function. (<b>optional</b>)
			 *
			 *  @throw connection_error If the client isn't connected.
			 */
			void
			pathImport( const std::string& path, const VoidSlot& slot,
			            const ErrorSlot& error = &Xmms::dummy_error ) const;

			/**
			 * @overload
			 * @note It takes a list of slots instead of just one slot.
			 */
			void
			pathImport( const std::string& path,
			            const std::list< VoidSlot >& slots,
			            const ErrorSlot& error = &Xmms::dummy_error ) const;

			/** Export a serverside playlist to a format 
			 *  that could be read from another mediaplayer.
			 *
			 *  @param playlist Name of the serverside playlist.
			 *  @param mime Mimetype of the export format.
			 *  @param slot Function pointer to a function taking a
			 *              const std::string& and returning a bool.
			 *  @param error Function pointer to an error callback
			 *               function. (<b>optional</b>)
			 *
			 *  @throw connection_error If the client isn't connected.
			 */
			void
			playlistExport( const std::string& playlist,
			                const std::string& mime,
			                const StringSlot& slot,
			                const ErrorSlot& error = &Xmms::dummy_error ) const;

			/**
			 * @overload
			 * @note It takes a list of slots instead of just one slot.
			 */
			void
			playlistExport( const std::string& playlist,
			                const std::string& mime,
			                const std::list< StringSlot >& slots,
			                const ErrorSlot& error = &Xmms::dummy_error ) const;

			/** Import a playlist from a playlist file.
			 *
			 *  @param playlist The name of the new playlist.
			 *  @param url URL to the playlist file.
			 *  @param slot Function pointer to a function returning a bool.
			 *  @param error Function pointer to an error callback
			 *               function. (<b>optional</b>)
			 *
			 *  @throw connection_error If the client isn't connected.
			 */
			void
			playlistImport( const std::string& playlist,
			                const std::string& url,
			                const VoidSlot& slot,
			                const ErrorSlot& error = &Xmms::dummy_error ) const;

			/**
			 * @overload
			 * @note It takes a list of slots instead of just one slot.
			 */
			void
			playlistImport( const std::string& playlist,
			                const std::string& url,
			                const std::list< VoidSlot >& slots,
			                const ErrorSlot& error = &Xmms::dummy_error ) const;

			/** Get a playlist.
			 *
			 *  @param playlist Name of the playlist.
			 *  @param slot Function pointer to a function taking a
			 *              const List< unsigned int >& and returning a bool.
			 *  @param error Function pointer to an error callback
			 *               function. (<b>optional</b>)
			 *
			 *  @throw connection_error If the client isn't connected.
			 */
			void
			playlistList( const std::string& playlist,
			              const UintListSlot& slot,
			              const ErrorSlot& error = &Xmms::dummy_error ) const;

			/**
			 * @overload
			 * @note It takes a list of slots instead of just one slot.
			 */
			void
			playlistList( const std::string& playlist,
			              const std::list< UintListSlot >& slots,
			              const ErrorSlot& error = &Xmms::dummy_error ) const;

			/** Load a playlist from the medialib 
			 *  to the current active playlist.
			 *
			 *  @param name Name of the playlist.
			 *  @param slot Function pointer to a function returning a bool.
			 *  @param error Function pointer to an error callback
			 *               function. (<b>optional</b>)
			 *
			 *  @throw connection_error If the client isn't connected.
			 */
			void
			playlistLoad( const std::string& name,
			              const VoidSlot& slot,
			              const ErrorSlot& error = &Xmms::dummy_error ) const;

			/**
			 * @overload
			 * @note It takes a list of slots instead of just one slot.
			 */
			void
			playlistLoad( const std::string& name,
			              const std::list< VoidSlot >& slots,
			              const ErrorSlot& error = &Xmms::dummy_error ) const;

			/** Remove a playlist from the medialib, 
			 *  keeping the songs of course.
			 *
			 *  @param playlist The playlist to remove.
			 *  @param slot Function pointer to a function returning a bool.
			 *  @param error Function pointer to an error callback
			 *               function. (<b>optional</b>)
			 *
			 *  @throw connection_error If the client isn't connected.
			 */
			void
			playlistRemove( const std::string& playlist,
			                const VoidSlot& slot,
			                const ErrorSlot& error = &Xmms::dummy_error ) const;

			/**
			 * @overload
			 * @note It takes a list of slots instead of just one slot.
			 */
			void
			playlistRemove( const std::string& playlist,
			                const std::list< VoidSlot >& slots,
			                const ErrorSlot& error = &Xmms::dummy_error ) const;

			/** Save the current playlist to a serverside playlist.
			 *
			 *  @param name Name to save the playlist as.
			 *  @param slot Function pointer to a function returning a bool.
			 *  @param error Function pointer to an error callback
			 *               function. (<b>optional</b>)
			 *
			 *  @throw connection_error If the client isn't connected.
			 */
			void
			playlistSaveCurrent( const std::string& name,
			                     const VoidSlot& slot,
			                     const ErrorSlot& error = &Xmms::dummy_error
			                   ) const;

			/**
			 * @overload
			 * @note It takes a list of slots instead of just one slot.
			 */
			void
			playlistSaveCurrent( const std::string& name,
			                     const std::list< VoidSlot >& slots,
			                     const ErrorSlot& error = &Xmms::dummy_error
			                   ) const;

			/** Returns a list of all available playlists.
			 *
			 *  @param slot Function pointer to a function taking a
			 *              const List< std::string >& and returning a bool.
			 *  @param error Function pointer to an error callback
			 *               function. (<b>optional</b>)
			 *
			 *  @throw connection_error If the client isn't connected.
			 */
			void
			playlistsList( const StringListSlot& slot,
			               const ErrorSlot& error = &Xmms::dummy_error ) const;

			/**
			 * @overload
			 * @note It takes a list of slots instead of just one slot.
			 */
			void
			playlistsList( const std::list< StringListSlot >& slots,
			               const ErrorSlot& error = &Xmms::dummy_error ) const;

			/** Rehash the medialib. 
			 *  This will check data in the medialib still 
			 *  is the same as the data in files.
			 *
			 *  @note This function is overloaded to provide one which takes
			 *  @c id and one which doesn't, if the one which doesn't
			 *  is used, the it will default (id = 0) rehashing the
			 *  whole medialib.
			 *
			 *  @param id The ID to rehash.
			 *  @param slot Function pointer to a function returning a bool.
			 *  @param error Function pointer to an error callback
			 *               function. (<b>optional</b>)
			 *
			 *  @throw connection_error If the client isn't connected.
			 */
			void
			rehash( unsigned int id, const VoidSlot& slot,
			        const ErrorSlot& error = &Xmms::dummy_error ) const;

			/**
			 * @overload
			 * @note It takes a list of slots instead of just one slot.
			 */
			void
			rehash( unsigned int id, const std::list< VoidSlot >& slot,
			        const ErrorSlot& error = &Xmms::dummy_error ) const;

			/**
			 * @overload
			 * @note It doesn't take the ID.
			 */
			void
			rehash( const VoidSlot& slot,
			        const ErrorSlot& error = &Xmms::dummy_error ) const;

			/**
			 * @overload
			 * @note It doesn't take the ID.
			 * @note It takes a list of slots instead of just one slot.
			 */
			void
			rehash( const std::list< VoidSlot >& slots,
			        const ErrorSlot& error = &Xmms::dummy_error ) const;

			/** Remove an entry from the medialib.
			 *  
			 *  @param id ID of the entry to remove.
			 *  @param slot Function pointer to a function returning a bool.
			 *  @param error Function pointer to an error callback
			 *               function. (<b>optional</b>)
			 *
			 *  @throw connection_error If the client isn't connected.
			 */
			void
			removeEntry( unsigned int id, const VoidSlot& slot,
			             const ErrorSlot& error = &Xmms::dummy_error ) const;

			/**
			 * @overload
			 * @note It takes a list of slots instead of just one slot.
			 */
			void
			removeEntry( unsigned int id, const std::list< VoidSlot >& slots,
			             const ErrorSlot& error = &Xmms::dummy_error ) const;

			/** Make a SQL query to the server medialib.
			 *  
			 *  @param query SQL query.
			 *  @param slot Function pointer to a function taking a
			 *              const List< Dict >& and returning a bool.
			 *  @param error Function pointer to an error callback
			 *               function. (<b>optional</b>)
			 *
			 *  @throw connection_error If the client isn't connected.
			 */
			void
			select( const std::string& query, const DictListSlot& slot,
			        const ErrorSlot& error = &Xmms::dummy_error ) const;

			/**
			 * @overload
			 * @note It takes a list of slots instead of just one slot.
			 */
			void
			select( const std::string& query,
			        const std::list< DictListSlot >& slot,
			        const ErrorSlot& error = &Xmms::dummy_error ) const;

			/** Request the medialib entry added broadcast.
			 *
			 *  This will be called if a new entry is added to
			 *  the medialib serverside.
			 * 
			 *  @param slot Function pointer to a function taking a
			 *              const unsigned int& and returning a bool.
			 *  @param error Function pointer to an error callback
			 *               function. (<b>optional</b>)
			 *
			 *  @throw connection_error If the client isn't connected.
			 */
			void
			broadcastEntryAdded( const UintSlot& slot,
			                     const ErrorSlot& error = &Xmms::dummy_error
			                   ) const;

			/**
			 * @overload
			 * @note It takes a list of slots instead of just one slot.
			 */
			void
			broadcastEntryAdded( const std::list< UintSlot >& slots,
			                     const ErrorSlot& error = &Xmms::dummy_error
			                   ) const;

			/** Request the medialib entry changed broadcast.
			 *
			 *  This will be called if a entry changes on the serverside.
			 *  The argument will be an medialib id. 
			 *  
			 *  @param slot Function pointer to a function taking a
			 *              const unsigned int& and returning a bool.
			 *  @param error Function pointer to an error callback
			 *               function. (<b>optional</b>)
			 *
			 *  @throw connection_error If the client isn't connected.
			 */
			void
			broadcastEntryChanged( const UintSlot& slot,
			                       const ErrorSlot& error = &Xmms::dummy_error
			                     ) const;

			/**
			 * @overload
			 * @note It takes a list of slots instead of just one slot.
			 */
			void
			broadcastEntryChanged( const std::list< UintSlot >& slot,
			                       const ErrorSlot& error = &Xmms::dummy_error
			                     ) const;

			/** Request the medialib_playlist_loaded broadcast.
			 *
			 *  This will be called if a playlist is loaded server-side.
			 *  The argument will be a string with the playlist name. 
			 *
			 *  @param slot Function pointer to a function taking a
			 *              const std::string& and returning a bool.
			 *  @param error Function pointer to an error callback
			 *               function. (<b>optional</b>)
			 *
			 *  @throw connection_error If the client isn't connected.
			 */
			void
			broadcastPlaylistLoaded( const StringSlot& slot,
			                         const ErrorSlot& error
			                         = &Xmms::dummy_error ) const;

			/**
			 * @overload
			 * @note It takes a list of slots instead of just one slot.
			 */
			void
			broadcastPlaylistLoaded( const std::list< StringSlot >& slots,
			                         const ErrorSlot& error
			                         = &Xmms::dummy_error ) const;

		/** @cond */
		private:
			friend class Client;
			Medialib( xmmsc_connection_t*& conn, bool& connected,
			          MainloopInterface*& ml );

			Medialib( const Medialib& src );
			Medialib& operator=( const Medialib& src );

			xmmsc_connection_t*& conn_;
			bool& connected_;
			MainloopInterface*& ml_;
		/** @endcond */

	};

}

#endif // XMMSCLIENTPP_MEDIALIB_H
