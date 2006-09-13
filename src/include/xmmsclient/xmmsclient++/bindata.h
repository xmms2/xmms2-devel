#ifndef XMMSCLIENTPP_BINDATA_H
#define XMMSCLIENTPP_BINDATA_H

#include <xmmsclient/xmmsclient.h>
#include <xmmsclient/xmmsclient++/mainloop.h>
#include <xmmsclient/xmmsclient++/typedefs.h>
#include <xmmsclient/xmmsclient++/helpers.h>

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
			std::string add( const Xmms::bin& data ) const;

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
			Xmms::bin retrieve( const std::string& hash ) const;

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
			void remove( const std::string& hash ) const;

			/** Add binary data to the servers bindata directory.
			 *  
			 *  @param data Binary data to be added.
			 *  @param slot Function pointer to a function taking
			 *              const std::string& and returning a bool.
			 *  @param error Function pointer to an error callback
			 *               function. (<b>optional</b>)
			 *
			 *  @throw connection_error If the client isn't connected.
			 */
			void add( const Xmms::bin& data, const StringSlot& slot,
			          const ErrorSlot& error = &Xmms::dummy_error ) const;

			/**
			 * @overload
			 * @note It takes a list of slots instead of just one slot.
			 */
			void add( const Xmms::bin& data,
			          const std::list< StringSlot >& slots,
			          const ErrorSlot& error = &Xmms::dummy_error ) const;

			/** Retrieve binary data from the servers bindata directory,
			 *  based on the hash.
			 *
			 *  @param hash Hash of the binary data to fetch.
			 *  @param slot Function pointer to a function taking
			 *              const Xmms::bin& and returning a bool.
			 *  @param error Function pointer to an error callback
			 *               function. (<b>optional</b>)
			 *
			 *  @throw connection_error If the client isn't connected.
			 */
			void retrieve( const std::string& hash, const BinSlot& slot, 
			               const ErrorSlot& error = &Xmms::dummy_error ) const;
			
			/**
			 * @overload
			 * @note It takes a list of slots instead of just one slot.
			 */
			void retrieve( const std::string& hash,
			               const std::list< BinSlot >& slots,
			               const ErrorSlot& error = &Xmms::dummy_error ) const;

			/** Remove the binary data associated with the hash.
			 *  
			 *  @param hash Hash of the binary data to remove.
			 *  @param slot Function pointer to a function returning a bool.
			 *  @param error Function pointer to an error callback
			 *               function. (<b>optional</b>)
			 *
			 *  @throw connection_error If the client isn't connected.
			 */
			void remove( const std::string& hash, const VoidSlot& slot,
			             const ErrorSlot& error = &Xmms::dummy_error ) const;

			/**
			 * @overload
			 * @note It takes a list of slots instead of just one slot.
			 */
			void remove( const std::string& hash,
			             const std::list< VoidSlot >& slots,
			             const ErrorSlot& error = &Xmms::dummy_error ) const;

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
