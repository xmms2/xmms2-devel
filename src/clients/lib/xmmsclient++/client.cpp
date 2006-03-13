#include <xmmsclient/xmmsclient++/client.h>
#include <xmmsclient/xmmsclient++/playback.h>
#include <xmmsclient/xmmsclient++/exceptions.h>
#include <xmmsclient/xmmsclient++/typedefs.h>

#include <string>
using std::string;

namespace Xmms 
{

	Client::Client( const string& name ) 
		: playback( &conn_ ), name_( name ), conn_(0), connected_( false )
	{
		conn_ = xmmsc_init( name.c_str() );
	}

	Client::~Client() 
	{
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
		}
	}

	const DictPtr Client::stats() const
	{
		if( !connected_ ) {
			throw connection_error( "Not connected" );
		}

		xmmsc_result_t* res = xmmsc_main_stats( conn_ );
		xmmsc_result_wait( res );

		if( xmmsc_result_iserror( res ) ) {
			// handle
		}

		char* ver = 0;
		if( !xmmsc_result_get_dict_entry_str( res, "version", &ver ) ) {
			// handle
		}

		int up = 0;
		if( !xmmsc_result_get_dict_entry_int32( res, "uptime", &up ) ) {
			// handle
		}

		DictPtr resultMap( new Dict() );
		boost::any version = string( ver );
		boost::any uptime = up;

		resultMap->operator[]("version") = version;
		resultMap->operator[]("uptime") = uptime;

		xmmsc_result_unref( res );
		return resultMap;

	}

	// TODO: ERROR CHECKING!
	const DictListPtr Client::pluginList(Plugins::Type type) const
	{
		if( !connected_ ) {
			throw connection_error( "Not connected" );
		}

		xmmsc_result_t* res = xmmsc_plugin_list( conn_, type ); 	

		xmmsc_result_wait( res );

		if( xmmsc_result_iserror( res ) ) {
			// handle
		}

		DictListPtr result( new DictList() );

		xmmsc_result_list_first( res );
		while( xmmsc_result_list_valid( res ) ) {
			
			char* cname = 0;			
			xmmsc_result_get_dict_entry_str( res, "name", &cname );

			char* cshortname = 0;
			xmmsc_result_get_dict_entry_str( res, "shortname", &cshortname );

			char* cdescription = 0;
			xmmsc_result_get_dict_entry_str( res, "description", 
			                                 &cdescription );

			unsigned int ctype = 0;
			xmmsc_result_get_dict_entry_uint32( res, "type", &ctype );

			boost::any name = string( cname );
			boost::any shortname = string( cshortname );
			boost::any description = string( cdescription );
			boost::any type = ctype;

			result->push_back( Dict() );
			result->back()["name"] = name;
			result->back()["shortname"] = shortname;
			result->back()["description"] = description;
			result->back()["type"] = type;

			xmmsc_result_list_next( res );

		}

		xmmsc_result_unref( res );

		return result;

	}

	MainLoop Client::getMainLoop() const {
		MainLoop ml;
		ml.addListener( new Listener( &conn_ ) );
		return ml;
	}
}
