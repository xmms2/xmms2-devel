#include <xmmsclient/xmmsclient.h>
#include <xmmsclient/xmmsclient++/dict.h>
#include <xmmsclient/xmmsclient++/exceptions.h>
#include <boost/any.hpp>
#include <string>
#include <iostream>

namespace Xmms
{
	Dict::Dict( xmmsc_result_t* res ) : result_( 0 )
	{
		if( xmmsc_result_iserror( res ) ) {
			throw result_error( xmmsc_result_get_error( res ) );
		}
		else if( xmmsc_result_get_type( res ) != XMMS_OBJECT_CMD_ARG_DICT ) {
			throw not_dict_error( "Result is not a dict" );
		}
		result_ = res;
		xmmsc_result_ref( result_ );
	}

	Dict::Dict( const Dict& dict ) : result_( dict.result_ )
	{
		xmmsc_result_ref( result_ );
	}

	Dict& Dict::operator=( const Dict& dict )
	{
		result_ = dict.result_;
		xmmsc_result_ref( result_ );
		return *this;
	}

	Dict::~Dict()
	{
		xmmsc_result_unref( result_ );
	}

	boost::any Dict::operator[]( const std::string& key ) const
	{
		boost::any value;
		switch( xmmsc_result_get_dict_entry_type( result_, key.c_str() ) )
		{
			case XMMSC_RESULT_VALUE_TYPE_UINT32: {

				unsigned int temp = 0;
				if( !xmmsc_result_get_dict_entry_uint32( result_, 
				                                         key.c_str(), 
				                                         &temp ) ) {
					// FIXME: handle error
				}
				value = temp;
				break;

			}
			case XMMSC_RESULT_VALUE_TYPE_INT32: {

				int temp = 0;
				if( !xmmsc_result_get_dict_entry_int32( result_, 
				                                        key.c_str(), 
				                                        &temp ) ) {
					// FIXME: handle error
				}
				value = temp;
				break;

			}
			case XMMSC_RESULT_VALUE_TYPE_STRING: {

				char* temp = 0;
				if( !xmmsc_result_get_dict_entry_str( result_, 
				                                      key.c_str(), 
				                                      &temp ) ) {
					// FIXME: handle error
				}
				value = std::string( temp );
				break;

			}
			case XMMSC_RESULT_VALUE_TYPE_NONE: {
				throw no_such_key_error( "No such key: " + key );
			}
			default: {
				// should never happen?
			}

		}

		return value;

	}

}

