#ifndef XMMSCLIENTPP_COLLECTION_H
#define XMMSCLIENTPP_COLLECTION_H

#include <xmmsclient/xmmsclient.h>
#include <xmmsclient/xmmsclient++/mainloop.h>
#include <xmmsclient/xmmsclient++/signal.h>
#include <xmmsclient/xmmsclient++/helpers.h>
#include <xmmsclient/xmmsclient++/typedefs.h>
#include <xmmsclient/xmmsclient++/dict.h>

#include <list>
#include <string>

#include <boost/shared_ptr.hpp>

namespace Xmms 
{

	// FIXME: Nicer way to do that?
	namespace Coll {
		class Coll;
	}

	typedef boost::shared_ptr< Coll::Coll > CollPtr;

	class Client;

	/** @class Collection collection.h "xmmsclient/xmmsclient++/collection.h"
	 *  @brief This class controls the collections on the server.
	 */
	class Collection
	{

		public:

			static CollPtr createColl( xmmsc_result_t* res );

			typedef xmmsc_coll_namespace_t Namespace;

			static const Namespace ALL;
			static const Namespace COLLECTIONS;
			static const Namespace PLAYLISTS;


			/** Destructor.
			 */
			virtual ~Collection();

			CollPtr
			get( const std::string& name, Namespace nsname ) const;

			List< std::string >
			list( Namespace nsname ) const;

			void
			save( const Coll::Coll& coll,
			      const std::string& name,
			      Namespace nsname ) const;

			void
			remove( const std::string& name, Namespace nsname ) const;

			List< std::string >
			find( unsigned int id, Namespace nsname ) const;

			CollPtr
			rename( const std::string& from_name,
			        const std::string& to_name,
			        Namespace nsname ) const;

			List< unsigned int >
			queryIds( const Coll::Coll& name,
			          const std::list<std::string>& order = std::list<std::string>(),
			          unsigned int limit_len = 0,
			          unsigned int limit_start = 0,
			          const std::list<std::string>& fetch = std::list<std::string>(),
			          const std::list<std::string>& group = std::list<std::string>()
			        ) const;

			List< Dict >
			queryInfos( const Coll::Coll& name,
			            const std::vector<std::string>& order,
			            unsigned int limit_len = 0,
			            unsigned int limit_start = 0) const;

			void
			broadcastCollectionChanged( const std::list< DictSlot >& slots,
			                            const ErrorSlot& error = &Xmms::dummy_error
			                          ) const;


		/** @cond */
		private:

			// Constructor, only to be called by Xmms::Client
			friend class Client;
			Collection( xmmsc_connection_t*& conn, bool& connected,
			            MainloopInterface*& ml );

			// Copy-constructor / operator=
			Collection( const Collection& src );
			Collection operator=( const Collection& src ) const;

			xmmsc_connection_t*& conn_;
			bool& connected_;
			MainloopInterface*& ml_;
		/** @endcond */

	};

}

#endif // XMMSCLIENTPP_COLLECTION_H
