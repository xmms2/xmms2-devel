#ifndef XMMSCLIENTPP_LIST_H
#define XMMSCLIENTPP_LIST_H

#include <xmmsclient/xmmsclient.h>
#include <boost/any.hpp>

namespace Xmms
{

	class List
	{

		public:
			List( xmmsc_result_t* result );
			~List();

			void first();
			const boost::any& operator*();
			const boost::any& operator->();
			void operator++();


			bool isValid();

		private:
			xmmsc_result_t* result_;
			boost::any contents_;

			bool constructed_;

			void constructContents();

	};

}

#endif // XMMSCLIENTPP_LIST_H
