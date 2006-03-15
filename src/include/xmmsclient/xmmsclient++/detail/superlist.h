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
				virtual ~SuperList();

				virtual void first();
				virtual void operator++();

				virtual bool isValid() const;

			protected:
				xmmsc_result_t* result_;
				bool constructed_;

				virtual void constructContents() = 0;

		};

		void dict_foreach( const void* key,
		                   xmmsc_result_value_type_t type,
						   const void* value,
						   void* udata );

		void propdict_foreach( const void* key,
		                       xmmsc_result_value_type_t type,
							   const void* value,
							   const char* source,
							   void* udata );

	}

}
#endif // XMMSCLIENTPP_SUPERLIST_H
