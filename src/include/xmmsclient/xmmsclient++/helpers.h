#ifndef XMMSCLIENTPP_HELPERS_HH
#define XMMSCLIENTPP_HELPERS_HH

#include <xmmsclient/xmmsclient.h>
#include <xmmsclient/xmmsclient++/exceptions.h>
#include <xmmsclient/xmmsclient++/mainloop.h>
#include <xmmsclient/xmmsclient++/signal.h>
#include <xmmsclient/xmmsclient++/dict.h>
#include <xmmsclient/xmmsclient++/typedefs.h>

#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <boost/signal.hpp>
#include <boost/type_traits/remove_pointer.hpp>

#include <string>
#include <list>

namespace Xmms
{

	/** @cond INTERNAL */

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
	inline void check( const MainloopInterface* const & ml )
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

	/** Convenience function to call a synchronous function.
	 *  @note does not unref the result
	 *
	 *  @param connected Connection status.
	 *  @param ml Currently running mainloop.
	 *  @param func Function pointer to the function to be called.
	 *
	 *  @return xmmsc_result_t* from the function called.
	 *
	 *  @throw connection_error If not connected.
	 *  @throw result_error If result returned was in error state.
	 *  @throw mainloop_running_error If the mainloop is running.
	 */
	inline xmmsc_result_t*
	call( bool connected, const MainloopInterface* const & ml, 
	      const boost::function< xmmsc_result_t*() >& func )
	{

		check( connected );
		check( ml );

		xmmsc_result_t* res = func();
		xmmsc_result_wait( res );

		check( res );
		return res;

	}

	/** Wrapper function for calling synchronous functions which
	 *  don't return a meaningful value.
	 *  Same as call but unrefs the result.
	 *  @see call
	 */
	inline void vCall( bool connected, const MainloopInterface* const & ml,
                       const boost::function< xmmsc_result_t*() >& func )
	{
		xmmsc_result_unref( call( connected, ml, func ) );
	}

	static bool dummy_error( const std::string& )
	{
		return false;
	}

	/** Convenience function for calling async functions and setting a callback.
	 *
	 *  @param connected Connection state.
	 *  @param func Function pointer to the function to be called.
	 *  @param slot Callback function pointer.
	 *  @param error Error callback function pointer.
	 *
	 *  @throw connection_error If not connected.
	 */ 
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
		SignalHolder::getInstance().addSignal( sig );
		xmmsc_result_t* res = func();
		xmmsc_result_notifier_set( res, Xmms::generic_callback< T >,
		                           static_cast< void* >( sig ) );
		xmmsc_result_unref( res );

	}

	/** Convenience function for calling async functions and settings callback.
	 *
	 *  Same as aCall but takes a list of callback function pointers.
	 *  @see aCall
	 */
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
		SignalHolder::getInstance().addSignal( sig );

		xmmsc_result_t* res = func();
		xmmsc_result_notifier_set( res, Xmms::generic_callback< T >,
		                           static_cast< void* >( sig ) );
		xmmsc_result_unref( res );

	}
	/** @endcond INTERNAL */

	/** @cond */
	template<typename Function> struct function_traits;

	template< typename R, typename C >
	struct function_traits< R (C::*)(void) >
	{
		typedef R result_type;
		typedef void arg1_type;
	};

	template< typename R, typename C, typename T1 >
	struct function_traits< R (C::*)(T1) >
	{
		typedef R result_type;
		typedef T1 arg1_type;
	};

	template< typename R, typename C, typename T1, typename T2 >
	struct function_traits< R (C::*)(T1, T2) >
	{
		typedef R result_type;
		typedef T1 arg1_type;
		typedef T2 arg2_type;
	};

	template< typename R, typename C, typename T1, typename T2, typename T3 >
	struct function_traits< R (C::*)(T1, T2, T3) >
	{
		typedef R result_type;
		typedef T1 arg1_type;
		typedef T2 arg2_type;
		typedef T3 arg3_type;
	};
	/** @endcond */

	/** Wrapper function for boost::bind to remove the repetition of _1.
	 *  
	 *  @param a1 Member function pointer. The function must have signature
	 *            bool( const T& ), where T is unsigned int, int, std::string,
	 *            Xmms::List, Xmms::Dict or Xmms::PropDict.
	 *  @param a2 Pointer to the object which owns the function pointer.
	 *
	 *  @return A function pointer to a member function a1 of class a2.
	 */
	template< typename A1, typename A2 >
	inline boost::function< bool( typename function_traits<
	                              typename boost::remove_pointer< A1 >::type
	                              >::arg1_type ) >
	bind( A1 a1, A2 a2 )
	{
		return boost::bind( a1, a2, _1 );
	}

	/** Wrapper function for boost::bind.
	 *  This is to be used with Dict::foreach when the foreach function is
	 *  a member function of some class.
	 *
	 *  @param a1 Member function pointer. The function must have signature
	 *            void( const std::string&, const Xmms::Dict::Variant& ).
	 *  @param a2 Pointer to the object which owns the function pointer.
	 *
	 *  @return A function pointer to a member function a1 of class a2
	 *          taking two arguments (std::string and Dict::Variant).
	 */
	template< typename A2 >
	inline boost::function< void( const std::string&, const Dict::Variant& ) >
	bind( void(boost::remove_pointer< A2 >::type::*a1)( const std::string&,
	                                                    const Dict::Variant& ),
	      A2 a2 )
	{
		return boost::bind( a1, a2, _1, _2 );
	} 

	/** Wrapper function for boost::bind.
	 *  This is to be used with PropDict::foreach when the foreach function is
	 *  a member function of some class.
	 *
	 *  @param a1 Member function pointer. The function must have signature
	 *            void( const std::string&, const Xmms::Dict::Variant&,
	 *                  const std::string& ).
	 *  @param a2 Pointer to the object which owns the function pointer.
	 *
	 *  @return A function pointer to a member function a1 of class a2
	 *          taking three arguments
	 *          (std::string, Dict::Variant and std::string)
	 */
	template< typename A2 >
	inline boost::function< void( const std::string&,
	                              const Dict::Variant&, const std::string& ) >
	bind( void(boost::remove_pointer< A2 >::type::*a1)( const std::string&,
	                                                    const Dict::Variant&,
	                                                    const std::string& ),
	      A2 a2 )
	{
		return boost::bind( a1, a2, _1, _2, _3 );
	}

	/** Wrapper function for boost::bind.
	 *  Trivial case.
	 *
	 *  @param a1 Member function pointer. The function must have signature
	 *            bool().
	 *  @param a2 Pointer to the object which owns the function pointer.
	 *
	 *  @return A function pointer to a member function a1 of class a2.
	 */
	template< typename A2 >
	inline boost::function< bool( void ) >
	bind( bool(boost::remove_pointer< A2 >::type::*a1)(), A2 a2 )
	{
		return boost::bind( a1, a2 );
	}

}

#endif // XMMSCLIENTPP_HELPERS_HH
