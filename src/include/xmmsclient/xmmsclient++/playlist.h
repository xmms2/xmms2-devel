#ifndef XMMSCLIENTPP_PLAYLIST_HH
#define XMMSCLIENTPP_PLAYLIST_HH

#include <xmmsclient/xmmsclient.h>
#include <xmmsclient/xmmsclient++/list.h>
#include <xmmsclient/xmmsclient++/mainloop.h>
#include <xmmsclient/xmmsclient++/signal.h>
#include <xmmsclient/xmmsclient++/helpers.h>
#include <string>

namespace Xmms
{
	
	class Client;

	/** @class Playlist playlist.h "xmmsclient/xmmsclient++/playlist.h"
	 *  @brief This class controls the playlist
	 */
	class Playlist
	{

		public:
			/** Destructor. 
			 *  Doesn't really do anything at this moment :) 
			 */
			virtual ~Playlist();

			/**	Add the url to the playlist.
			 *  The url should be absolute to the server-side. 
			 *  Note that you will have to include the protocol 
			 *  for the url to. ie: file://mp3/my_mp3s/first.mp3.
			 *  
			 *  @param url file to be added
			 *
			 *  @throw connection_error If the client isn't connected.
			 *  @throw mainloop_running_error If a mainloop is running -
			 *  sync functions can't be called when mainloop is running. This
			 *  is only thrown if the programmer is careless or doesn't know
			 *  what he/she's doing. (logic_error)
			 *  @throw result_error If the operation failed.
			 */
			void add( const std::string& url ) const;
			
			/** Add a medialib id to the playlist.
			 *
			 *  @param id A medialib ID
			 *
			 *  @throw connection_error If the client isn't connected.
			 *  @throw mainloop_running_error If a mainloop is running -
			 *  sync functions can't be called when mainloop is running. This
			 *  is only thrown if the programmer is careless or doesn't know
			 *  what he/she's doing. (logic_error)
			 *  @throw result_error If the operation failed.
			 */
			void add( const unsigned int id ) const;

			/** Clears the current playlist.
			 *
			 *  @throw connection_error If the client isn't connected.
			 *  @throw mainloop_running_error If a mainloop is running -
			 *  sync functions can't be called when mainloop is running. This
			 *  is only thrown if the programmer is careless or doesn't know
			 *  what he/she's doing. (logic_error)
			 *  @throw result_error If the operation failed.
			 */
			void clear() const;

			/** Retrieve the current position in the playlist.
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
			unsigned int currentPos() const;

			/** Insert entry at given position in playlist.
			 *  
			 *  @param pos A position in the playlist.
			 *  @param url The URL to insert.
			 *
			 *  @throw connection_error If the client isn't connected.
			 *  @throw mainloop_running_error If a mainloop is running -
			 *  sync functions can't be called when mainloop is running. This
			 *  is only thrown if the programmer is careless or doesn't know
			 *  what he/she's doing. (logic_error)
			 *  @throw result_error If the operation failed.
			 */
			void insert( int pos, const std::string& url ) const;

			/** Insert a medialib ID at given position in playlist.
			 *
			 *  @param pos A position in the playlist.
			 *  @param id A medialib ID.
			 *
			 *  @throw connection_error If the client isn't connected.
			 *  @throw mainloop_running_error If a mainloop is running -
			 *  sync functions can't be called when mainloop is running. This
			 *  is only thrown if the programmer is careless or doesn't know
			 *  what he/she's doing. (logic_error)
			 *  @throw result_error If the operation failed.
			 */
			void insert( int pos, unsigned int id ) const;

			/** Retrieve the current playlist.
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
			List< unsigned int > list() const;

			/** Move a playlist entry to a new position (absolute move).
			 * 
			 *  @param curpos Position of the entry to be moved.
			 *  @param newpos Position where the entry should be moved to.
			 *
			 *  @throw connection_error If the client isn't connected.
			 *  @throw mainloop_running_error If a mainloop is running -
			 *  sync functions can't be called when mainloop is running. This
			 *  is only thrown if the programmer is careless or doesn't know
			 *  what he/she's doing. (logic_error)
			 *  @throw result_error If the operation failed.
			 */
			void move( unsigned int curpos, unsigned int newpos ) const;

			/** Remove an entry from the playlist.
			 * 
			 *  @param pos The position that should be 
			 *             removed from the playlist.
			 *
			 *  @throw connection_error If the client isn't connected.
			 *  @throw mainloop_running_error If a mainloop is running -
			 *  sync functions can't be called when mainloop is running. This
			 *  is only thrown if the programmer is careless or doesn't know
			 *  what he/she's doing. (logic_error)
			 *  @throw result_error If the operation failed.
			 */
			void remove( unsigned int pos ) const;

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

			/** Shuffles the current playlist.
			 */
			void shuffle() const;

			/** Sorts the playlist according to the property.
			 * 
			 *  @param property Property to sort by.
			 *
			 *  @throw connection_error If the client isn't connected.
			 *  @throw mainloop_running_error If a mainloop is running -
			 *  sync functions can't be called when mainloop is running. This
			 *  is only thrown if the programmer is careless or doesn't know
			 *  what he/she's doing. (logic_error)
			 *  @throw result_error If the operation failed.
			 */
			void sort( const std::string& property ) const;

			void
			add( const std::string& url,
			     const VoidSlot& slot,
			     const ErrorSlot& error = &Xmms::dummy_error ) const;

			void
			add( const std::string& url,
			     const std::list<VoidSlot>& slots,
			     const ErrorSlot& error = &Xmms::dummy_error ) const;

			void
			add( const unsigned int id,
			     const VoidSlot& slot,
			     const ErrorSlot& error = &Xmms::dummy_error ) const;

			void
			add( const unsigned int id,
			     const std::list<VoidSlot>& slots,
			     const ErrorSlot& error = &Xmms::dummy_error ) const;

			void 
			clear( const VoidSlot& slot,
			       const ErrorSlot& error = &Xmms::dummy_error ) const;

			void
			clear( const std::list<VoidSlot>& slots,
			       const ErrorSlot& error = &Xmms::dummy_error ) const;

			void
			currentPos( const UintSlot& slot,
			            const ErrorSlot& error = &Xmms::dummy_error ) const;

			void
			currentPos( const std::list<UintSlot>& slots,
			            const ErrorSlot& error = &Xmms::dummy_error ) const;

			void
			insert( int pos, const std::string& url,
			        const VoidSlot& slot,
					const ErrorSlot& error = &Xmms::dummy_error ) const;

			void
			insert( int pos, const std::string& url,
			        const std::list< VoidSlot >& slots,
			        const ErrorSlot& error = &Xmms::dummy_error ) const;

			void
			insert( int pos, unsigned int id,
			        const VoidSlot& slot,
			        const ErrorSlot& error = &Xmms::dummy_error ) const;

			void
			insert( int pos, unsigned int id,
			        const std::list< VoidSlot >& slots,
			        const ErrorSlot& error = &Xmms::dummy_error ) const;

			void
			list( const UintListSlot& slot,
			      const ErrorSlot& error = &Xmms::dummy_error ) const;

			void
			list( const std::list< UintListSlot >& slots,
			      const ErrorSlot& error = &Xmms::dummy_error ) const;

			void
			move( unsigned int curpos, unsigned int newpos,
			      const VoidSlot& slot,
			      const ErrorSlot& error = &Xmms::dummy_error ) const;

			void
			move( unsigned int curpos, unsigned int newpos,
			      const std::list< VoidSlot >& slots,
			      const ErrorSlot& error = &Xmms::dummy_error ) const;

			void
			remove( unsigned int pos,
			        const VoidSlot& slot,
					const ErrorSlot& error = &Xmms::dummy_error ) const;

			void
			remove( unsigned int pops,
			        const std::list< VoidSlot >& slots,
			        const ErrorSlot& error = &Xmms::dummy_error ) const;

			void
			setNext( unsigned int pos,
			         const UintSlot& slot,
					 const ErrorSlot& error = &Xmms::dummy_error ) const;

			void
			setNext( unsigned int pos,
			         const std::list< UintSlot >& slots,
			         const ErrorSlot& error = &Xmms::dummy_error ) const;

			void
			setNextRel( signed int pos,
			            const UintSlot& slot,
			            const ErrorSlot& error = &Xmms::dummy_error ) const;

			void
			setNextRel( signed int pos,
			            const std::list< UintSlot >& slots,
			            const ErrorSlot& error = &Xmms::dummy_error ) const;

			void
			shuffle( const VoidSlot& slot,
			         const ErrorSlot& error = &Xmms::dummy_error ) const;

			void
			shuffle( const std::list< VoidSlot >& slots,
			         const ErrorSlot& error = &Xmms::dummy_error ) const;

			void
			sort( const std::string& property,
			      const VoidSlot& slot,
			      const ErrorSlot& error = &Xmms::dummy_error ) const;

			void
			sort( const std::string& property,
			      const std::list< VoidSlot >& slots,
			      const ErrorSlot& error = &Xmms::dummy_error ) const;

			void
			broadcastChanged( const DictSlot& slot,
			                  const ErrorSlot& error = &Xmms::dummy_error
			                ) const; 

			void
			broadcastChanged( const std::list< DictSlot >& slots,
			                  const ErrorSlot& error = &Xmms::dummy_error
			                ) const;

			void
			broadcastCurrentPos( const UintSlot& slot,
			                     const ErrorSlot& error = &Xmms::dummy_error
			                   ) const;

			void
			broadcastCurrentPos( const std::list< UintSlot >& slot,
			                     const ErrorSlot& error = &Xmms::dummy_error
			                   ) const;

		/** @cond */
		private:
			friend class Client;
			Playlist( xmmsc_connection_t*& conn, bool& connected,
			          MainLoop*& ml );

			Playlist( const Playlist& src );
			Playlist& operator=( const Playlist& src );

			xmmsc_connection_t*& conn_;
			bool& connected_;
			MainLoop*& ml_;
		/** @endcond */

	};

}

#endif // XMMSCLIENTPP_PLAYLIST_HH
