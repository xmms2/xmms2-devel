#include <xmmsclient/xmmsclient.h>

#include <xmmsclient/xmmsclient++/list.h>
#include <xmmsclient/xmmsclient++/exceptions.h>

#include <boost/any.hpp>

#include <string>
#include <iostream>

namespace Xmms
{

	Detail::SuperList::SuperList( xmmsc_result_t* result )
		: result_( result ), constructed_( false )
	{

		if( xmmsc_result_iserror( result_ ) ) {
			throw result_error( xmmsc_result_get_error( result_ ) );
		}
		if( !xmmsc_result_is_list( result_ ) ) {
			throw not_list_error( "Provided result is not a list" );
		}

	}

	Detail::SuperList::~SuperList()
	{
		xmmsc_result_unref( result_ );
	}

	void Detail::SuperList::first()
	{

		if( !xmmsc_result_list_first( result_ ) ) {
			// throw something...
		}
		constructed_ = false;

	}

	void Detail::SuperList::operator++()
	{
		if( !xmmsc_result_list_next( result_ ) ) {
			// throw
		}
		constructed_ = false;
	}

	bool Detail::SuperList::isValid() const
	{
		return xmmsc_result_list_valid( result_ );
	}

	void Detail::dict_foreach( const void* key,
	                           xmmsc_result_value_type_t type,
							   const void* value,
							   void* udata )
	{

		Dict* dict( static_cast< Dict* >( udata ) );
		boost::any temp;
		switch( type ) {
			case XMMSC_RESULT_VALUE_TYPE_STRING: {
				temp = std::string( static_cast< const char* >( value ) );
				break;
			}
			case XMMSC_RESULT_VALUE_TYPE_UINT32: {
				temp = reinterpret_cast< const uint32_t >( value );
				break;
			}
			case XMMSC_RESULT_VALUE_TYPE_INT32: {
				temp = reinterpret_cast< const int32_t >( value );
				break;
			}
			case XMMSC_RESULT_VALUE_TYPE_NONE: {
				// FIXME: probably not the right thing to do
				return;
			}
		}

		(*dict)[ static_cast< const char* >( key ) ] = temp;

	}

	void Detail::propdict_foreach( const void* key,
	                               xmmsc_result_value_type_t type,
								   const void* value,
								   const char* source,
								   void* udata )
	{
	}

}
