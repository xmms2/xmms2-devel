/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2012 XMMS2 Team
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

#include <xmmsclient/xmmsclient.h>

#include <xmmsclient/xmmsclient++/client.h>
#include <xmmsclient/xmmsclient++/playback.h>
#include <xmmsclient/xmmsclient++/playlist.h>
#include <xmmsclient/xmmsclient++/medialib.h>
#include <xmmsclient/xmmsclient++/xform.h>
#include <xmmsclient/xmmsclient++/config.h>
#include <xmmsclient/xmmsclient++/stats.h>
#include <xmmsclient/xmmsclient++/exceptions.h>
#include <xmmsclient/xmmsclient++/typedefs.h>
#include <xmmsclient/xmmsclient++/helpers.h>
#include <xmmsclient/xmmsclient++/signal.h>
#include <xmmsclient/xmmsclient++/result.h>

#include <boost/bind.hpp>

#include <string>

namespace Xmms 
{

	Client::Client( const std::string& name ) 
		: bindata(    conn_, connected_, mainloop_ ),
		  playback(   conn_, connected_, mainloop_ ),
		  playlist(   conn_, connected_, mainloop_ ),
		  medialib(   conn_, connected_, mainloop_ ),
		  config(     conn_, connected_, mainloop_ ),
		  stats(      conn_, connected_, mainloop_ ),
		  xform(      conn_, connected_, mainloop_ ),
		  collection( conn_, connected_, mainloop_ ),
		  name_( name ), conn_(0), connected_( false ),
		  mainloop_( 0 ), listener_( 0 ), quitSignal_( 0 ), dc_( 0 )
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
		if( conn_ ) {
			xmmsc_unref( conn_ );
		}
	}

	void Client::connect( const char* ipcpath )
	{

		if( !connected_ ) {
			if( !conn_ ) {
				conn_ = xmmsc_init( name_.c_str() );
			}
			if( !xmmsc_connect( conn_, ipcpath ) ) {

				throw connection_error( xmmsc_get_last_error( conn_ ) );

			}
			connected_ = true;
		}

		if( mainloop_ && !listener_ && typeid(mainloop_) == typeid(MainLoop) ) {
			listener_ = new Listener( conn_ );
			dynamic_cast<MainLoop*>(mainloop_)->addListener( listener_ );
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

	QuitSignal&
	Client::broadcastQuit()
	{

		check( connected_ );
		if( !quitSignal_ ) {
			xmmsc_result_t* res = xmmsc_broadcast_quit( conn_ );
			quitSignal_ = new QuitSignal( res, mainloop_ );
		}
		return *quitSignal_;

	}

	MainloopInterface& Client::getMainLoop() 
	{

		if( !mainloop_ ) {
			mainloop_ = new MainLoop( conn_ );
			listener_ = new Listener( conn_ );
			broadcastQuit().connect( boost::bind( &Client::quitHandler,
			                                      this, _1 ) );
			setDisconnectCallback( boost::bind( &Client::dcHandler, this ) );
			dynamic_cast<MainLoop*>(mainloop_)->addListener( listener_ );
		}
		return *mainloop_;

	}

	void Client::setMainloop( MainloopInterface* ml )
	{

		if( mainloop_ ) {
			delete mainloop_;
		}
		mainloop_ = ml;
		broadcastQuit().connect( boost::bind( &Client::quitHandler,
		                                      this, _1 ) );
		setDisconnectCallback( boost::bind( &Client::dcHandler, this ) );

	}

	void
	Client::setDisconnectCallback( const DisconnectCallback::value_type& slot )
	{

		if( !dc_ ) {
			dc_ = new DisconnectCallback;
			xmmsc_disconnect_callback_set( conn_, &disconnect_callback,
			                               static_cast< void* >( dc_ ) );
		}
		dc_->push_back( slot );

	}

	bool Client::isConnected() const
	{
		return connected_;
	}

	std::string Client::getLastError() const
	{
		return std::string( xmmsc_get_last_error( conn_ ) );
	}

	bool Client::quitHandler( const int& /*time*/ )
	{
		return true;
	}

	void Client::dcHandler()
	{
		connected_ = false;
		if( mainloop_ && listener_ ) {
			dynamic_cast<MainLoop*>(mainloop_)->removeListener( listener_ );
			delete listener_; listener_ = 0;
		}
		else if( mainloop_ ) {
			delete mainloop_; mainloop_ = 0;
		}

		SignalHolder::getInstance().deleteAll();
		xmmsc_unref( conn_ ); conn_ = 0;
	}

}
