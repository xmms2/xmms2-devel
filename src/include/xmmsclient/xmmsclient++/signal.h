#ifndef XMMSCLIENTPP_SIGNAL_H
#define XMMSCLIENTPP_SIGNAL_H

#include <xmmsclient/xmmsclient.h>
#include <boost/signal.hpp>
#include <boost/shared_ptr.hpp>
#include <string>

namespace Xmms
{

	template< typename T >
	struct Signal
	{
		boost::signal< bool( const std::string& ) > error_signal;
		boost::signal< bool( const T& ) > signal;
	};

	template< typename T >
	inline boost::shared_ptr< T > extract_value( xmmsc_result_t* res );

	template<>
	inline boost::shared_ptr< unsigned int >
	extract_value( xmmsc_result_t* res )
	{
		unsigned int* temp = new unsigned int;
		xmmsc_result_get_uint( res, temp );
		return boost::shared_ptr< unsigned int >( temp );
	}

	template<>
	inline boost::shared_ptr< int >
	extract_value( xmmsc_result_t* res )
	{
		int* temp = new int;
		xmmsc_result_get_int( res, temp );
		return boost::shared_ptr< int >( temp );
	}

	template<>
	inline boost::shared_ptr< std::string >
	extract_value( xmmsc_result_t* res )
	{
		char* temp = 0;
		xmmsc_result_get_string( res, &temp );
		return boost::shared_ptr< std::string >( new std::string( temp ) );
	}

	template<>
	inline boost::shared_ptr< Dict >
	extract_value( xmmsc_result_t* res )
	{
		return boost::shared_ptr< Dict >( new Dict( res ) );
	}

	template<>
	inline boost::shared_ptr< List< unsigned int > >
	extract_value( xmmsc_result_t* res )
	{
		return boost::shared_ptr< List< unsigned int > >( 
		       new List< unsigned int >( res ) );
	}
	
	template<>
	inline boost::shared_ptr< List< int > >
	extract_value( xmmsc_result_t* res )
	{
		return boost::shared_ptr< List< int > >(
		       new List< int >( res ) );
	}

	template<>
	inline boost::shared_ptr< List< std::string > >
	extract_value( xmmsc_result_t* res )
	{
		return boost::shared_ptr< List< std::string > >(
		       new List< std::string >( res ) );
	}

	template<>
	inline boost::shared_ptr< List< Dict > >
	extract_value( xmmsc_result_t* res )
	{
		return boost::shared_ptr< List< Dict > >(
		       new List< Dict >( res ) );
	}

	template<>
	inline boost::shared_ptr< PropDict >
	extract_value( xmmsc_result_t* res )
	{
		return boost::shared_ptr< PropDict >( new PropDict( res ) );
	}

	template< typename T >
	void generic_callback( xmmsc_result_t* res, void* userdata )
	{

		Signal< T >* data = static_cast< Signal< T >* >( userdata );

		bool ret = false;
		if( xmmsc_result_iserror( res ) ) {

			std::string error( xmmsc_result_get_error( res ) );
			ret = data->error_signal( error );

		}
		else {

			ret = data->signal( *extract_value< T >( res ) );

		}

		if( ret && xmmsc_result_get_type( res ) == XMMSC_RESULT_CLASS_SIGNAL ) {

			xmmsc_result_t* newres = xmmsc_result_restart( res );
			xmmsc_result_unref( newres );

		}
		else {

			if( xmmsc_result_get_type( res ) == XMMSC_RESULT_CLASS_BROADCAST ) {
				xmmsc_result_disconnect( res );
			}

			delete data;

		}

		xmmsc_result_unref( res );

	}

}

#endif // XMMSCLIENTPP_SIGNAL_H
