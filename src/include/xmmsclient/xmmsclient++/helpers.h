#ifndef XMMSCLIENTPP_HELPERS_HH
#define XMMSCLIENTPP_HELPERS_HH

#include <xmmsclient/xmmsclient.h>
#include <xmmsclient/xmmsclient++/exceptions.h>
#include <xmmsclient/xmmsclient++/mainloop.h>

#include <string>

namespace Xmms
{

	/** Asserts connection state.
	 *  This function is a convenience function to reduce writing when
	 *  checking for connection in clientlib functions.
	 *  
	 *  @param connected connection state.
	 *
	 *  @throw connection_error If not connected.
	 */
	inline void Assert( bool connected )
	{
		if( !connected ) {
			throw connection_error( "Not connected" );
		}
	}

	/** Asserts mainloop state.
	 *  Convenience function for sync functions to check that mainloop is
	 *  not running.
	 *  
	 *  @param ml Mainloop to check.
	 *
	 *  @throw mainloop_running_error If the mainloop is up and running.
	 */
	inline void Assert( const MainLoop* const & ml )
	{
		if( ml && ml->isRunning() ) {
			throw mainloop_running_error( "Cannot perform synchronized "
			                              "operations when mainloop is "
			                              "running." );
		}
	}

	/** Assert result for errors.
	 *  Convenience function to check if result is in error state.
	 *  @note result is unreffed if it's in error state.
	 *
	 *  @param res xmmsc_result_t* to check.
	 *
	 *  @throw result_error If the result is in error state.
	 */
	inline void Assert( xmmsc_result_t*& res )
	{
		if( xmmsc_result_iserror( res ) ) {
			std::string error( xmmsc_result_get_error( res ) );
			xmmsc_result_unref( res );
			throw result_error( error );
		}
	}

}

#endif // XMMSCLIENTPP_HELPERS_HH
