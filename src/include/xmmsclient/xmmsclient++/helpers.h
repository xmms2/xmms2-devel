#ifndef XMMSCLIENTPP_HELPERS_HH
#define XMMSCLIENTPP_HELPERS_HH

#include <xmmsclient/xmmsclient.h>
#include <xmmsclient/xmmsclient++/exceptions.h>
#include <xmmsclient/xmmsclient++/mainloop.h>
#include <xmmsclient/xmmsclient++/signal.h>
#include <xmmsclient/xmmsclient++/dict.h>
#include <xmmsclient/xmmsclient++/typedefs.h>

#include <boost/function.hpp>
#include <boost/signal.hpp>

#include <string>
#include <algorithm>
#include <list>

namespace Xmms
{

	/** Checks connection state.
	 *  This function is a convenience function to reduce writing when
	 *  checking for connection in clientlib functions.
	 *  
	 *  @param connected connection state.
	 *
	 *  @throw connection_error If not connected.
	 */
	inline void check( bool connected )
	{
		if( !connected ) {
			throw connection_error( "Not connected" );
		}
	}

	/** Checks mainloop state.
	 *  Convenience function for sync functions to check that mainloop is
	 *  not running.
	 *  
	 *  @param ml Mainloop to check.
	 *
	 *  @throw mainloop_running_error If the mainloop is up and running.
	 */
	inline void check( const MainLoop* const & ml )
	{
		if( ml && ml->isRunning() ) {
			throw mainloop_running_error( "Cannot perform synchronized "
			                              "operations when mainloop is "
			                              "running." );
		}
	}

	/** Checks result for errors.
	 *  Convenience function to check if result is in error state.
	 *  @note result is unreffed if it's in error state.
	 *
	 *  @param res xmmsc_result_t* to check.
	 *
	 *  @throw result_error If the result is in error state.
	 */
	inline void check( xmmsc_result_t*& res )
	{
		if( xmmsc_result_iserror( res ) ) {
			std::string error( xmmsc_result_get_error( res ) );
			xmmsc_result_unref( res );
			throw result_error( error );
		}
	}

	inline xmmsc_result_t*
	call( bool connected, const MainLoop* const & ml, 
	      const boost::function< xmmsc_result_t*() >& func )
	{

		check( connected );
		check( ml );

		xmmsc_result_t* res = func();
		xmmsc_result_wait( res );

		check( res );
		return res;

	}

	inline void vCall( bool connected, const MainLoop* const & ml,
                       const boost::function< xmmsc_result_t*() >& func )
	{
		xmmsc_result_unref( call( connected, ml, func ) );
	}

	static bool dummy_error( const std::string& )
	{
		return false;
	}

	template< typename T >
	inline void aCall( bool connected, 
	                   const boost::function< xmmsc_result_t*() >& func,
	                   const typename Signal<T>::signal_t::slot_type& slot,
	                   const error_sig::slot_type& error )
	{

		check( connected );

		Xmms::Signal< T >* sig = new Xmms::Signal< T >;
		sig->signal.connect( slot );
		sig->error_signal.connect( error );
		xmmsc_result_t* res = func();
		xmmsc_result_notifier_set( res, Xmms::generic_callback< T >,
		                           static_cast< void* >( sig ) );
		xmmsc_result_unref( res );

	}

	template< typename T >
	inline void 
	aCall( bool connected,
	       const boost::function< xmmsc_result_t*() >& func,
	       const std::list< typename Signal<T>::signal_t::slot_type >& slots,
		   const error_sig::slot_type& error )
	{

		check( connected );

		Xmms::Signal< T >* sig = new Xmms::Signal< T >;

		typename std::list< 
			typename Signal<T>::signal_t::slot_type >::const_iterator i;
		for( i = slots.begin(); i != slots.end(); ++i )
		{
			sig->signal.connect( *i );
		}
		sig->error_signal.connect( error );

		xmmsc_result_t* res = func();
		xmmsc_result_notifier_set( res, Xmms::generic_callback< T >,
		                           static_cast< void* >( sig ) );
		xmmsc_result_unref( res );

	}

}

#endif // XMMSCLIENTPP_HELPERS_HH
