#include <xmmsclient/xmmsclient.h>

#include <xmmsclient/xmmsclient++/list.h>
#include <xmmsclient/xmmsclient++/exceptions.h>

#include <boost/any.hpp>

#include <string>

namespace Xmms
{

	List::List( xmmsc_result_t* result )
		: result_( result ), constructed_( false )
	{

		if( xmmsc_result_iserror( result_ ) == 1 ) {
			throw result_error( xmmsc_result_get_error( result_ ) );
		}
		if( xmmsc_result_is_list( result_ ) == 0 ) {
			throw not_list_error( "Provided result is not a list" );
		}

	}

	List::~List()
	{
		xmmsc_result_unref( result_ );
	}

	void List::first()
	{

		if( xmmsc_result_list_first( result_ ) == 0 ) {
			// throw something...
		}
		constructed_ = false;

	}

	const boost::any& List::operator*()
	{
		constructContents();
		return contents_;
	}

	const boost::any& List::operator->()
	{
		constructContents();
		return contents_;
	}

	void List::operator++()
	{
		if( xmmsc_result_list_next( result_ ) == 0 ) {
			// throw
		}
		constructed_ = false;
	}

	bool List::isValid()
	{

		if( xmmsc_result_list_valid( result_ ) == 0 ) {
			return false;
		}
		return true;

	}

	void List::constructContents()
	{
		if( constructed_ ) {
			return;
		}
		switch( xmmsc_result_get_type( result_ ) )
		{
			case XMMS_OBJECT_CMD_ARG_UINT32: {
				unsigned int temp = 0;
				if( xmmsc_result_get_uint( result_, &temp ) == 0 ) {
					// throw something
				}
				contents_ = temp;
				break;
			}
			case XMMS_OBJECT_CMD_ARG_INT32: {
				int temp = 0;
				if( xmmsc_result_get_int( result_, &temp ) == 0 ) {
					// throw something
				}
				contents_ = temp;
				break;
			}
			case XMMS_OBJECT_CMD_ARG_STRING: {
				char* temp = 0;
				if( xmmsc_result_get_string( result_, &temp ) == 0 ) {
					// throw something
				}
				contents_ = std::string( temp );
				break;
			}
			case XMMS_OBJECT_CMD_ARG_DICT: {
				// TODO: implement dict foreach function
				break;
			}
			case XMMS_OBJECT_CMD_ARG_PROPDICT: {
				// TODO: implement propdict foreach function
				break;
			}
			case XMMS_OBJECT_CMD_ARG_LIST: {
				contents_ = List( result_ );
				break;
			}
			default: {
				throw no_result_type_error( "Result is of no type" );
			}

		}

		constructed_ = true;
	}

}
