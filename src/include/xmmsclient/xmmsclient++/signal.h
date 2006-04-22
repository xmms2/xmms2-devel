#ifndef XMMSCLIENTPP_SIGNAL_H
#define XMMSCLIENTPP_SIGNAL_H

#include <xmmsclient/xmmsclient.h>
#include <boost/signal.hpp>
#include <string>

namespace Xmms
{

	template< typename T >
	struct Signal
	{
		typedef boost::signal< bool( const T& ) > signal_t;
		boost::signal< bool( const std::string& ) > error_signal;
		signal_t signal;
	};

	template<>
	struct Signal< void >
	{
		typedef boost::signal< bool() > signal_t;
		boost::signal< bool( const std::string& ) > error_signal;
		signal_t signal;
	};

	template< typename T >
	inline T* extract_value( xmmsc_result_t* res )
	{
		return new T( res );
	}

	template<>
	inline unsigned int*
	extract_value( xmmsc_result_t* res )
	{
		unsigned int* temp = new unsigned int;
		xmmsc_result_get_uint( res, temp );
		return temp;
	}

	template<>
	inline int*
	extract_value( xmmsc_result_t* res )
	{
		int* temp = new int;
		xmmsc_result_get_int( res, temp );
		return temp;
	}

	template<>
	inline std::string*
	extract_value( xmmsc_result_t* res )
	{
		char* temp = 0;
		xmmsc_result_get_string( res, &temp );
		return new std::string( temp );
	}

	template< typename T >
	static bool
	callSignal( const Signal< T >* sig, xmmsc_result_t*& res )
	{

		T* value = extract_value< T >( res );
		bool ret = sig->signal( *value );
		delete value;
		return ret;

	}

	template<>
	static bool
	callSignal( const Signal< void >* sig, xmmsc_result_t*& /* res */)
	{
		return sig->signal();
	}

	template< typename T >
	inline void generic_callback( xmmsc_result_t* res, void* userdata )
	{

		Signal< T >* data = static_cast< Signal< T >* >( userdata );

		bool ret = false;
		if( xmmsc_result_iserror( res ) ) {

			std::string error( xmmsc_result_get_error( res ) );
			ret = data->error_signal( error );

		}
		else {

			ret = callSignal( data, res );

		}

		if( ret && 
		    xmmsc_result_get_class( res ) == XMMSC_RESULT_CLASS_SIGNAL ) {

			xmmsc_result_t* newres = xmmsc_result_restart( res );
			xmmsc_result_unref( newres );

		}
		else {

			if( xmmsc_result_get_class( res ) == 
			    XMMSC_RESULT_CLASS_BROADCAST ) {

				xmmsc_result_disconnect( res );

			}

			delete data;

		}

		xmmsc_result_unref( res );

	}

}

#endif // XMMSCLIENTPP_SIGNAL_H
