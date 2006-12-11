#ifndef XMMSCLIENTPP_MEDIALIB_H
#define XMMSCLIENTPP_MEDIALIB_H

#include <xmmsclient/xmmsclient.h>
#include <xmmsclient/xmmsclient++/mainloop.h>
#include <xmmsclient/xmmsclient++/dict.h>
#include <xmmsclient/xmmsclient++/list.h>
#include <xmmsclient/xmmsclient++/typedefs.h>
#include <xmmsclient/xmmsclient++/helpers.h>

#include <string>
#include <list>

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

			/** Add a URL with arguments to the medialib.
			 *  If you want to add multiple files you should call pathImport.
			 *
			 *  @param url URL to add to the medialib.
			 *  @param args List of strings used as arguments.
			 *
			 *  @throw connection_error If the client isn't connected.
			 *  @throw mainloop_running_error If a mainloop is running -
			 *  sync functions can't be called when mainloop is running. This
			 *  is only thrown if the programmer is careless or doesn't know
			 *  what he/she's doing. (logic_error)
			 *  @throw result_error If the operation failed.
			 */
			void addEntry( const std::string& url,
			               const std::list< std::string >& args ) const;

			/** Add a URL to the medialib.
			 *  Same as #addEntry but takes a encoded URL instead.
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
			void addEntryEncoded( const std::string& url ) const;

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

			/**
			 * @overload
			 * @note It takes a int instead of string
			 */
			void entryPropertySet( unsigned int id,
			                       const std::string& key,
			                       int value,
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

			/** Import all files recursively from the 
			 *  directory passed as argument.
			 *
			 *  same as #pathImport but takes a encoded path instead.
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
			void pathImportEncoded( const std::string& path ) const;

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

			/** Add a URL with arguments to the medialib.
			 *  If you want to add multiple files you should call pathImport.
			 *
			 *  @param url URL to add to the medialib.
			 *  @param args List of strings used as arguments.
			 *  @param slot Function pointer to a function returning a bool.
			 *  @param error Function pointer to an error callback
			 *               function. (<b>optional</b>)
			 *
			 *  @throw connection_error If the client isn't connected.
			 */
			void
			addEntry( const std::string& url,
			          const std::list< std::string >& args,
			          const VoidSlot& slot,
			          const ErrorSlot& error = &Xmms::dummy_error ) const;

			/** Add a URL to the medialib.
			 *  Same as #addEntry but takes a encoded URL insetad.
			 *
			 *  @param url URL to add to the medialib.
			 *  @param slot Function pointer to a function returning a bool.
			 *  @param error Function pointer to an error callback
			 *               function. (<b>optional</b>)
			 *
			 *  @throw connection_error If the client isn't connected.
			 */
			void
			addEntryEncoded( const std::string& url,
			                 const VoidSlot& slot,
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
			 * @note It doesn't take the source.
			 */
			void
			entryPropertyRemove( unsigned int id, const std::string& key,
			                     const VoidSlot& slot,
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
			 * @note It doesn't take the source.
			 */
			void
			entryPropertySet( unsigned int id, const std::string& key,
			                  const std::string& value, const VoidSlot& slot,
			                  const ErrorSlot& error = &Xmms::dummy_error
			                ) const;

			/**
			 * @overload
			 * @note It takes a int instead of a string
			 */
			void
			entryPropertySet( unsigned int id, const std::string& key,
			                  int value,
			                  const std::string& source,
			                  const VoidSlot& slot,
			                  const ErrorSlot& error = &Xmms::dummy_error
			                ) const;

			/**
			 * @overload
			 * @note It takes a int instead of a string
			 * @note It doesn't take the source.
			 */
			void
			entryPropertySet( unsigned int id, const std::string& key,
			                  int value, const VoidSlot& slot,
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

			/** Import all files recursively from the 
			 *  directory passed as argument.
			 *
			 *  same as #pathImport but takes a encoded url instead.
			 *
			 *  @param path Directory to import.
			 *  @param slot Function pointer to a function returning a bool.
			 *  @param error Function pointer to an error callback
			 *               function. (<b>optional</b>)
			 *
			 *  @throw connection_error If the client isn't connected.
			 */
			void
			pathImportEncoded( const std::string& path, const VoidSlot& slot,
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
			 * @note It doesn't take the ID.
			 */
			void
			rehash( const VoidSlot& slot,
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
