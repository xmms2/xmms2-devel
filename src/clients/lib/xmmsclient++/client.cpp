#include <xmmsclient/xmmsclient.h>

#include <xmmsclient/xmmsclient++/client.h>
#include <xmmsclient/xmmsclient++/playback.h>
#include <xmmsclient/xmmsclient++/playlist.h>
#include <xmmsclient/xmmsclient++/medialib.h>
#include <xmmsclient/xmmsclient++/exceptions.h>
#include <xmmsclient/xmmsclient++/typedefs.h>
#include <xmmsclient/xmmsclient++/dict.h>
#include <xmmsclient/xmmsclient++/helpers.h>
#include <xmmsclient/xmmsclient++/signal.h>

#include <boost/bind.hpp>

#include <list>
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

	void Client::stats(const Signal<Dict>::signal_t::slot_type& slot,
	                   const error_sig::slot_type& error ) const
	{

		aCall<Dict>( connected_, boost::bind( xmmsc_main_stats, conn_ ), 
		             slot, error );
	}

	void
	Client::stats(const std::list< Signal<Dict>::signal_t::slot_type >& slots,
	              const error_sig::slot_type& error ) const
	{
		aCall<Dict>( connected_, boost::bind( xmmsc_main_stats, conn_ ),
		             slots, error );
	}

	void 
	Client::pluginList(const Signal<DictList>::signal_t::slot_type& slot,
	                   const error_sig::slot_type& error ) const
	{
		pluginList( Plugins::ALL, slot, error );
	}

	void
	Client::pluginList(Plugins::Type type,
	                   const Signal<DictList>::signal_t::slot_type& slot,
	                   const error_sig::slot_type& error ) const
	{
		aCall<DictList>( connected_, 
		                 boost::bind( xmmsc_plugin_list, conn_, type ),
		                 slot, error );
	}

	void
	Client::pluginList(const std::list<
	                         Signal<DictList>::signal_t::slot_type >& slots,
	                   const error_sig::slot_type& error ) const
	{
		pluginList( Plugins::ALL, slots, error );
	}

	void
	Client::pluginList(Plugins::Type type,
	                   const std::list<
	                         Signal<DictList>::signal_t::slot_type >& slots,
	                   const error_sig::slot_type& error ) const
	{
		aCall<DictList>( connected_,
		                 boost::bind( xmmsc_plugin_list, conn_, type ),
		                 slots, error );
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
