/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2023 XMMS2 Team
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
#include <xmmsclient/xmmsclient++/exceptions.h>
#include <boost/function.hpp>
#include <string>
#include <list>
#include <iostream>
#include <deque>

#include <boost/scoped_ptr.hpp>

namespace Xmms
{

	/** @cond INTERNAL */
	namespace Coll {
		class Coll;
	}

	/** @class SignalInterface
	 *  This is here only to unify all Signal classes so that they can be
	 *  put in one list.
	 */
	struct SignalInterface
	{
		typedef std::deque< boost::function< bool( const std::string& ) > > error_sig;

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
		typedef std::deque< boost::function< bool( T& ) > > signal_t;

		error_sig error_signal;
		signal_t signal;

	};

	/** @class Signal
	 *  Specialized Signal for void signals.
	 */
	template<>
	struct Signal< void > : public SignalInterface
	{
		typedef std::deque< boost::function< bool() > > signal_t;

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
	 *  xmmsv_t.
	 *  @note return value must be deleted
	 */
	template< typename T >
	inline T* extract_value( xmmsv_t* val )
	{
		return new T( val );
	}

	template<>
	inline int*
	extract_value( xmmsv_t* val )
	{
		int* temp = new int;
		xmmsv_get_int( val, temp );
		return temp;
	}
	
	template<>
	inline std::basic_string<unsigned char>*
	extract_value( xmmsv_t* val )
	{
		const unsigned char* temp = 0;
		unsigned int len = 0;
		xmmsv_get_bin( val, &temp, &len );
		return new std::basic_string<unsigned char>( temp, len );
	}

	template<>
	inline std::string*
	extract_value( xmmsv_t* val )
	{
		const char* temp = 0;
		xmmsv_get_string( val, &temp );
		return new std::string( temp );
	}

	template<>
	inline xmms_playback_status_t*
	extract_value( xmmsv_t* val )
	{
		int32_t temp = 0;
		xmmsv_get_int( val, &temp );
		xmms_playback_status_t* result = new xmms_playback_status_t;
		*result = static_cast< xmms_playback_status_t >( temp );
		return result;
	}

	template<>
	inline xmms_mediainfo_reader_status_t*
	extract_value( xmmsv_t* val )
	{
		int temp = 0;
		xmmsv_get_int( val, &temp );
		xmms_mediainfo_reader_status_t* result
			= new xmms_mediainfo_reader_status_t;
		*result = static_cast< xmms_mediainfo_reader_status_t >( temp );
		return result;
	}

	Coll::Coll*
	extract_collection( xmmsv_t* val );

	template<>
	inline Coll::Coll*
	extract_value( xmmsv_t* val )
	{
		return extract_collection( val );
	}


	/** Templated function to handle the value extraction, signal calling
	 *  and deletion of the extracted value.
	 */
	template< typename T >
	inline bool
	callSignal( const Signal< T >* sig, xmmsv_t*& val )
	{
		boost::scoped_ptr< T > value( extract_value< T >( val ) );
		bool ret = true;
		for( typename Signal<T>::signal_t::const_iterator i = sig->signal.begin();
		     i != sig->signal.end(); ++i )
		{
			ret &= (*i)(*value);
		}
		return ret;
	}

	/** Specialized version of the templated callSignal.
	 */
	template<>
	inline bool
	callSignal( const Signal< void >* sig, xmmsv_t*& /* val */)
	{
		bool ret = true;
		for( Signal<void>::signal_t::const_iterator i = sig->signal.begin();
		     i != sig->signal.end(); ++i )
		{
			ret &= (*i)();
		}
		return ret;
	}
	inline bool
	callError( const SignalInterface::error_sig& error_signal,
			   const std::string& error )
	{
		bool ret = true;
		for( SignalInterface::error_sig::const_iterator i = error_signal.begin();
		     i != error_signal.end(); ++i )
		{
			ret &= (*i)( error );
		}
		return ret;
	}


	/** Called on the notifier udata when an xmmsc_result_t is freed.
	 */
	inline void
	freeSignal( void* notifier_data )
	{
		SignalInterface *sig = static_cast< SignalInterface* >( notifier_data );
		SignalHolder::getInstance().removeSignal( sig );
	}

	/** Generic callback function to handle signal calling, error checking,
	 *  signal renewing, broadcast disconnection and Signal deletion.
	 */
	template< typename T >
	inline int
	generic_callback( xmmsv_t* val, void* userdata )
	{
		if( !userdata ) {
			return 0; // don't restart
		}

		Signal< T >* data = static_cast< Signal< T >* >( userdata );

		bool ret = false;
		if( xmmsv_is_error( val ) ) {
			const char *buf;
			xmmsv_get_error( val, &buf );

			std::string error( buf );
			if( !data->error_signal.empty() ) {
				ret = callError(data->error_signal, error);
			}

		}
		else {

			if( !data->signal.empty() ) {
				try {
					ret = callSignal( data, val );
				}
				catch( exception& e ) {

					std::exception* ee = 0;
					if( (ee = dynamic_cast<std::exception*>( &e )) &&
					    !data->error_signal.empty() ) {
						ret = callError( data->error_signal, ee->what() );
					}
					else {
						throw;
					}

				}

			}

		}

		// Note: the signal is removed from the SignalHolder by
		// freeSignal when the result is freed.

		return ret;
	}

	typedef std::deque< boost::function< void() > > DisconnectCallback;

	void disconnect_callback( void* userdata );
	/** @endcond INTERNAL */

}

#endif // XMMSCLIENTPP_SIGNAL_H
