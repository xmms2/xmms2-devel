#include <xmmsclient/xmmsclient.h>

#include <xmmsclient/xmmsclient++/client.h>
#include <xmmsclient/xmmsclient++/playback.h>
#include <xmmsclient/xmmsclient++/exceptions.h>
#include <xmmsclient/xmmsclient++/typedefs.h>
#include <xmmsclient/xmmsclient++/dict.h>

#include <string>
using std::string;

namespace Xmms 
{

	Client::Client( const string& name ) 
		: playback( &conn_ ), name_( name ), conn_(0), connected_( false ),
		  mainloop_( 0 )
	{
		conn_ = xmmsc_init( name.c_str() );
	}

	Client::~Client() 
	{
		if( mainloop_ ) {
			delete mainloop_;
		}
		xmmsc_unref( conn_ );
	}

	void Client::connect( const string& ipcpath )
	{

		if( !connected_ ) {
			if( !xmmsc_connect(conn_, 
			                   ipcpath.empty() ? 0 : ipcpath.c_str() ) ) {

				throw connection_error( xmmsc_get_last_error( conn_ ) );

			}
			connected_ = true;
		}

	}

	void Client::quit()
	{
		if( connected_ ) {
			xmmsc_result_t* res = xmmsc_quit( conn_ );
			xmmsc_result_unref( res );
			connected_ = false;
		}
	}

	const Dict Client::stats() const
	{
		if( !connected_ ) {
			throw connection_error( "Not connected" );
		}
		if( mainloop_ && mainloop_->isRunning() ) {
			// FIXME: throw something, this function can't be called
			// when the mainloop is running...
		}

		xmmsc_result_t* res = xmmsc_main_stats( conn_ );
		xmmsc_result_wait( res );

		try {

			Dict resultMap( res );

			xmmsc_result_unref( res );
			return resultMap;

		}
		catch( ... ) {

			xmmsc_result_unref( res );
			throw;

		}

	}

	const DictList Client::pluginList(Plugins::Type type) const
	{
		if( !connected_ ) {
			throw connection_error( "Not connected" );
		}

		xmmsc_result_t* res = xmmsc_plugin_list( conn_, type ); 	

		xmmsc_result_wait( res );

		try {

			List< Dict > resultList( res );

			xmmsc_result_unref( res );
			return resultList;

		}
		catch( ... ) {

			xmmsc_result_unref( res );
			throw;

		}

	}

	MainLoop& Client::getMainLoop() 
	{

		if( !mainloop_ ) {
			mainloop_ = new MainLoop();
			mainloop_->addListener( new Listener( &conn_ ) );
		}
		return *mainloop_;

	}
}
