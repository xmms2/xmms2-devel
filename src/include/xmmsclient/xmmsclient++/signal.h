/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2008 XMMS2 Team
 *
 *  PLUGINS ARE NOT CONSIDERED TO BE DERIVED WORK !!!
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 */

#ifndef XMMSCLIENTPP_SIGNAL_H
#define XMMSCLIENTPP_SIGNAL_H

#include <xmmsclient/xmmsclient.h>
#include <boost/signal.hpp>
#include <string>
#include <list>
#include <iostream>

#include <boost/scoped_ptr.hpp>

namespace Xmms
{

	/** @cond INTERNAL */
	namespace Coll {
		class Coll;
	}

	typedef boost::signal< bool( const std::string& ) > error_sig;

	/** @class SignalInterface
	 *  This is here only to unify all Signal classes so that they can be
	 *  put in one list.
	 */
	struct SignalInterface
	{

		public:
			SignalInterface() {}
			virtual ~SignalInterface() {}

	};

	/** @class Signal
	 *  Holds error signal and normal signal
	 */
	template< typename T >
	struct Signal : public SignalInterface
	{
		typedef boost::signal< bool( const T& ) > signal_t;

		error_sig error_signal;
		signal_t signal;

	};

	/** @class Signal
	 *  Specialized Signal for void signals.
	 */
	template<>
	struct Signal< void > : public SignalInterface
	{
		typedef boost::signal< bool() > signal_t;

		error_sig error_signal;
		signal_t signal;

	};

	/** @class SignalHolder
	 *  @brief Holds a list of Signal classes and deletes them when requested
	 *         or when the program quits and there still are some in the list.
	 *  @note A singleton.
	 */
	class Client;
	class SignalHolder
	{

		public:
			/** Get the instance of this class.
			 *  @return a SignalHolder instance.
			 */
			static SignalHolder& getInstance();

			/** Add a signal to the list.
			 *  @param sig A pointer to signal to add.
			 *  @note You must @b not delete the signal afterwards.
			 */
			void addSignal( SignalInterface* sig );

			/** Remove a signal from the list.
			 *  @param sig A pointer to signal to remove.
			 *  @note This will delete the signal too.
			 */
			void removeSignal( SignalInterface* sig );

			/** Cleans up existing signals
			 */
			~SignalHolder();

		/** @cond */
		private:
			friend class Client;
			SignalHolder()
			{
			}
			SignalHolder( SignalHolder& src );
			SignalHolder& operator=( SignalHolder& src );
			void deleteAll();

			std::list< SignalInterface* > signals_;
		/** @endcond */

	};

	/** Templated functions for extracting the correct value from
	 *  xmmsc_result_t.
	 *  @note return value must be deleted
	 */
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
	inline std::basic_string<unsigned char>*
	extract_value( xmmsc_result_t* res )
	{
		unsigned char* temp = 0;
		unsigned int len = 0;
		xmmsc_result_get_bin( res, &temp, &len );
		return new std::basic_string<unsigned char>( temp, len );
	}

	template<>
	inline std::string*
	extract_value( xmmsc_result_t* res )
	{
		const char* temp = 0;
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
		int temp = 0;
		xmmsc_result_get_int( res, &temp );
		xmms_mediainfo_reader_status_t* result
			= new xmms_mediainfo_reader_status_t;
		*result = static_cast< xmms_mediainfo_reader_status_t >( temp );
		return result;
	}

	Coll::Coll*
	extract_collection( xmmsc_result_t* res );

	template<>
	inline Coll::Coll*
	extract_value( xmmsc_result_t* res )
	{
		return extract_collection( res );
	}


	/** Templated function to handle the value extraction, signal calling
	 *  and deletion of the extracted value.
	 */
	template< typename T >
	inline bool
	callSignal( const Signal< T >* sig, xmmsc_result_t*& res )
	{

		boost::scoped_ptr< T > value( extract_value< T >( res ) );
		bool ret = sig->signal( *value );
		return ret;

	}

	/** Specialized version of the templated callSignal.
	 */
	template<>
	inline bool
	callSignal( const Signal< void >* sig, xmmsc_result_t*& /* res */)
	{
		return sig->signal();
	}

	/** Generic callback function to handle signal calling, error checking,
	 *  signal renewing, broadcast disconnection and Signal deletion.
	 */
	template< typename T >
	inline void generic_callback( xmmsc_result_t* res, void* userdata )
	{
		if( !userdata ) {
			xmmsc_result_unref( res );
			return;
		}

		Signal< T >* data = static_cast< Signal< T >* >( userdata );

		bool ret = false;
		if( xmmsc_result_iserror( res ) ) {

			std::string error( xmmsc_result_get_error( res ) );
			if( !data->error_signal.empty() ) {
				ret = data->error_signal( error );
			}

		}
		else {

			if( !data->signal.empty() ) {
				try {
					ret = callSignal( data, res );
				}
				catch( std::exception& e ) {

					if( !data->error_signal.empty() ) {
						ret = data->error_signal( e.what() );
					}

				}

			}

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

		if( xmmsc_result_get_class( res ) != XMMSC_RESULT_CLASS_BROADCAST ) {

			xmmsc_result_unref( res );

		}

	}

	typedef boost::signal< void() > DisconnectCallback;

	void disconnect_callback( void* userdata );
	/** @endcond INTERNAL */

}

#endif // XMMSCLIENTPP_SIGNAL_H
