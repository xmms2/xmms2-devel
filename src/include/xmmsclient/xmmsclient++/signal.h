#ifndef XMMSCLIENTPP_SIGNAL_H
#define XMMSCLIENTPP_SIGNAL_H

#include <xmmsclient/xmmsclient.h>
#include <boost/signal.hpp>
#include <string>
#include <list>

namespace Xmms
{

	typedef boost::signal< bool( const std::string& ) > error_sig;

	struct SignalInterface
	{

		public:
			SignalInterface() {}
			virtual ~SignalInterface() {}

	};

	template< typename T >
	struct Signal : public SignalInterface
	{
		typedef boost::signal< bool( const T& ) > signal_t;

		error_sig error_signal;
		signal_t signal;

	};

	template<>
	struct Signal< void > : public SignalInterface
	{
		typedef boost::signal< bool() > signal_t;

		error_sig error_signal;
		signal_t signal;

	};

	class Client;
	class SignalHolder
	{

		public:
			static SignalHolder& getInstance();

			void addSignal( SignalInterface* sig );

			void removeSignal( SignalInterface* sig );

			~SignalHolder();

		private:
			friend class Client;
			SignalHolder()
			{
			}
			SignalHolder( SignalHolder& src );
			SignalHolder& operator=( SignalHolder& src );
			void deleteAll();

			std::list< SignalInterface* > signals_;

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

	template<>
	inline xmms_playback_status_t*
	extract_value( xmmsc_result_t* res )
	{
		unsigned int temp = 0;
		xmmsc_result_get_uint( res, &temp );
		xmms_playback_status_t* result = new xmms_playback_status_t;
		*result = static_cast< xmms_playback_status_t >( temp );
		return result;
	}

	template<>
	inline xmms_mediainfo_reader_status_t*
	extract_value( xmmsc_result_t* res )
	{
		unsigned int temp = 0;
		xmmsc_result_get_uint( res, &temp );
		xmms_mediainfo_reader_status_t* result
			= new xmms_mediainfo_reader_status_t;
		*result = static_cast< xmms_mediainfo_reader_status_t >( temp );
		return result;
	}

	template< typename T >
	inline bool
	callSignal( const Signal< T >* sig, xmmsc_result_t*& res )
	{

		T* value = extract_value< T >( res );
		bool ret = sig->signal( *value );
		delete value;
		return ret;

	}

	template<>
	inline bool
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
		else if( !ret || 
		         xmmsc_result_get_class( res ) == XMMSC_RESULT_CLASS_DEFAULT) {

			if( xmmsc_result_get_class( res ) == 
			    XMMSC_RESULT_CLASS_BROADCAST ) {

				xmmsc_result_disconnect( res );

			}

			SignalHolder::getInstance().removeSignal( data ); data = 0;

		}

		xmmsc_result_unref( res );

	}

}

#endif // XMMSCLIENTPP_SIGNAL_H
