#ifndef XMMSCLIENTPP_XFORM_H
#define XMMSCLIENTPP_XFORM_H

#include <xmmsclient/xmmsclient.h>
#include <xmmsclient/xmmsclient++/mainloop.h>
#include <xmmsclient/xmmsclient++/dict.h>
#include <xmmsclient/xmmsclient++/list.h>
#include <xmmsclient/xmmsclient++/typedefs.h>
#include <xmmsclient/xmmsclient++/helpers.h>
#include <xmmsclient/xmmsclient++/signal.h>

#include <string>

namespace Xmms
{

	class Client;

	/** @class Xform xform.h "xmmsclient/xmmsclient++/xform.h"
	 *  @brief This class controls the xform object.
	 */
	class Xform
	{

		public:
			/** Destructor. */
			~Xform();

			List< Dict > browse( const std::string& url ) const;
			void browse ( const std::string& url, const DictListSlot& slot,
						  const ErrorSlot& error = &Xmms::dummy_error ) const;

		/** @cond */
		private:
			friend class Client;
			Xform ( xmmsc_connection_t*& conn, bool& connected,
					MainloopInterface*& ml );

			Xform( const Xform& src );
			Xform operator=( const Xform& src );

			xmmsc_connection_t*& conn_;
			bool& connected_;
			MainloopInterface*& ml_;
		/** @endcond */
	};
}

#endif
