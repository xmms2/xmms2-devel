#ifndef XMMSCLIENTPP_MEDIALIB_H
#define XMMSCLIENTPP_MEDIALIB_H

#include <xmmsclient/xmmsclient.h>
#include <xmmsclient/xmmsclient++/mainloop.h>
#include <xmmsclient/xmmsclient++/dict.h>
#include <xmmsclient/xmmsclient++/list.h>

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
			 *  client/<clientname> if not provided.
			 *
			 *  @param id Entry ID.
			 *  @param key Field key to remove.
			 *  @param source Source for the entry. (optional)
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
			 *  client/<clientname> if not provided.
			 *
			 *  @param id Entry ID.
			 *  @param key Field key to set.
			 *  @param value to set.
			 *  @param source Source for the value. (optional)
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
			 *  @param id The ID to rehash. (optional, 
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
			 *  @param entry ID of the entry to remove.
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

		/** @cond */
		private:
			friend class Client;
			Medialib( xmmsc_connection_t*& conn, bool& connected,
			          MainLoop*& ml );

			Medialib( const Medialib& src );
			Medialib& operator=( const Medialib& src );

			xmmsc_connection_t*& conn_;
			bool& connected_;
			MainLoop*& ml_;
		/** @endcond */

	};

}

#endif // XMMSCLIENTPP_MEDIALIB_H
