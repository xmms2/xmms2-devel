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
		  mainloop_( 0 ), listener_( 0 ), quitSignal_( 0 )
	{
		conn_ = xmmsc_init( name.c_str() );
	}

	Client::~Client() 
	{
		if( mainloop_ ) {
			// Also deletes listener_
			delete mainloop_;
		}
		if( quitSignal_ ) {
			delete quitSignal_;
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

		if( mainloop_ && !listener_ ) {
			listener_ = new Listener( conn_ );
			mainloop_->addListener( listener_ );
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

	void Client::stats(const DictSlot& slot,
	                   const ErrorSlot& error ) const
	{

		aCall<Dict>( connected_, boost::bind( xmmsc_main_stats, conn_ ), 
		             slot, error );
	}

	void
	Client::stats(const std::list< DictSlot >& slots,
	              const ErrorSlot& error ) const
	{
		aCall<Dict>( connected_, boost::bind( xmmsc_main_stats, conn_ ),
		             slots, error );
	}

	void 
	Client::pluginList(const DictListSlot& slot,
	                   const ErrorSlot& error ) const
	{
		pluginList( Plugins::ALL, slot, error );
	}

	void
	Client::pluginList(Plugins::Type type,
	                   const DictListSlot& slot,
	                   const ErrorSlot& error ) const
	{
		aCall<DictList>( connected_, 
		                 boost::bind( xmmsc_plugin_list, conn_, type ),
		                 slot, error );
	}

	void
	Client::pluginList(const std::list< DictListSlot >& slots,
	                   const ErrorSlot& error ) const
	{
		pluginList( Plugins::ALL, slots, error );
	}

	void
	Client::pluginList(Plugins::Type type,
	                   const std::list< DictListSlot >& slots,
	                   const ErrorSlot& error ) const
	{
		aCall<DictList>( connected_,
		                 boost::bind( xmmsc_plugin_list, conn_, type ),
		                 slots, error );
	}

	void
	Client::broadcastQuit( const UintSlot& slot, const ErrorSlot& error )
	{

		check( connected_ );
		if( !quitSignal_ ) {
			quitSignal_ = new Signal<unsigned int>;
			xmmsc_result_t* res = xmmsc_broadcast_quit( conn_ );
			xmmsc_result_notifier_set( res, 
			                           Xmms::generic_callback<unsigned int>,
			                           static_cast< void* >( quitSignal_ ) );
			xmmsc_result_unref( res );
		}
		quitSignal_->signal.connect( slot );
		quitSignal_->error_signal.connect( error );

	}

	MainLoop& Client::getMainLoop() 
	{

		if( !mainloop_ ) {
			mainloop_ = new MainLoop();
			listener_ = new Listener( conn_ );
			broadcastQuit( boost::bind( &Client::quitHandler, this, _1 ) );
			mainloop_->addListener( listener_ );
		}
		return *mainloop_;

	}

	bool Client::isConnected() const
	{
		return connected_;
	}

	bool Client::quitHandler( const unsigned int& /*time*/ )
	{
		connected_ = false;
		if( mainloop_ ) {
			mainloop_->removeListener( listener_ );
			delete listener_; listener_ = 0;
		}

		SignalHolder::getInstance().deleteAll();
		return true;
	}

}
