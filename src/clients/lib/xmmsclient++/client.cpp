#include <xmmsclient/xmmsclient.h>

#include <xmmsclient/xmmsclient++/client.h>
#include <xmmsclient/xmmsclient++/playback.h>
#include <xmmsclient/xmmsclient++/playlist.h>
#include <xmmsclient/xmmsclient++/medialib.h>
#include <xmmsclient/xmmsclient++/exceptions.h>
#include <xmmsclient/xmmsclient++/typedefs.h>
#include <xmmsclient/xmmsclient++/dict.h>
#include <xmmsclient/xmmsclient++/helpers.h>

#include <boost/bind.hpp>

#include <string>
using std::string;

namespace Xmms 
{

	Client::Client( const string& name ) 
		: playback( conn_, connected_, mainloop_ ), 
	      playlist( conn_, connected_, mainloop_ ), 
		  medialib( conn_, connected_, mainloop_ ),
		  name_( name ), conn_(0), connected_( false ),
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

		xmmsc_result_t* res = 
		    call( connected_, mainloop_,
		          boost::bind( xmmsc_main_stats, conn_ ) );

		Dict resultMap( res );

		xmmsc_result_unref( res );
		return resultMap;

	}

	const DictList Client::pluginList(Plugins::Type type) const
	{

		xmmsc_result_t* res = 
		    call( connected_, mainloop_,
		          boost::bind( xmmsc_plugin_list, conn_, type ) ); 	
		
		List< Dict > resultList( res );

		xmmsc_result_unref( res );
		return resultList;

	}

	MainLoop& Client::getMainLoop() 
	{

		if( !mainloop_ ) {
			mainloop_ = new MainLoop();
			mainloop_->addListener( new Listener( conn_ ) );
		}
		return *mainloop_;

	}

	bool Client::isConnected() const
	{
		return connected_;
	}
}
