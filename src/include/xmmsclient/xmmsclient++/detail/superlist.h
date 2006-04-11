#ifndef XMMSCLIENTPP_SUPERLIST_H
#define XMMSCLIENTPP_SUPERLIST_H

#include <xmmsclient/xmmsclient.h>

namespace Xmms
{

	namespace Detail
	{
		class SuperList
		{

			public:
				SuperList( xmmsc_result_t* result );
				SuperList( const SuperList& list );
				virtual SuperList& operator=( const SuperList& list );
				virtual ~SuperList();

				virtual void first();
				virtual void operator++();

				virtual bool isValid() const;

			protected:
				xmmsc_result_t* result_;
				bool constructed_;

				virtual void constructContents() = 0;

		};

	}

}
#endif // XMMSCLIENTPP_SUPERLIST_H
