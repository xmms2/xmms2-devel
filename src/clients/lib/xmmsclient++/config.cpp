#include <xmmsclient/xmmsclient.h>
#include <xmmsclient/xmmsclient++/client.h>
#include <xmmsclient/xmmsclient++/config.h>
#include <xmmsclient/xmmsclient++/signal.h>
#include <xmmsclient/xmmsclient++/helpers.h>
#include <xmmsclient/xmmsclient++/mainloop.h>
#include <xmmsclient/xmmsclient++/typedefs.h>

#include <boost/bind.hpp>

#include <list>
#include <string>

namespace Xmms
{
	
	Config::~Config()
	{
	}

	void Config::valueRegister( const std::string& name,
	                            const std::string& defval ) const
	{
		vCall( connected_, ml_,
		       boost::bind( xmmsc_configval_register, conn_,
		                    name.c_str(), defval.c_str() ) );
	}

	void Config::valueSet( const std::string& key,
	                       const std::string& value ) const
	{
		vCall( connected_, ml_,
		       boost::bind( xmmsc_configval_set, conn_,
		                    key.c_str(), value.c_str() ) );
	}

	const std::string Config::valueGet( const std::string& key ) const
	{
		xmmsc_result_t* res = 
		    call( connected_, ml_,
		          boost::bind( xmmsc_configval_get, conn_, key.c_str() ) );

		char* temp = 0;
		xmmsc_result_get_string( res, &temp );

		std::string result( temp );
		xmmsc_result_unref( res );

		return result;
	}

	Dict Config::valueList() const
	{
		xmmsc_result_t* res = call( connected_, ml_,
		                            boost::bind( xmmsc_configval_list, conn_ ));
		Dict result( res );
		xmmsc_result_unref( res );

		return result;
	}


	void
	Config::valueRegister( const std::string& name, const std::string& defval,
	                       const VoidSlot& slot,
	                       const ErrorSlot& error ) const
	{
		aCall<void>( connected_,
		             boost::bind( xmmsc_configval_register, conn_,
		                          name.c_str(), defval.c_str() ),
		             slot, error );
	}

	void
	Config::valueSet( const std::string& key, const std::string& value,
	                  const VoidSlot& slot,
	                  const ErrorSlot& error ) const
	{
		aCall<void>( connected_,
		             boost::bind( xmmsc_configval_set, conn_,
		                          key.c_str(), value.c_str() ),
		             slot, error );
	}

	void
	Config::valueGet( const std::string& key,
	                  const StringSlot& slot,
	                  const ErrorSlot& error ) const
	{
		using boost::bind;
		aCall<std::string>( connected_,
		                    bind( xmmsc_configval_get, conn_, key.c_str() ),
		                    slot, error );
	}

	void
	Config::valueList( const DictSlot& slot,
	                   const ErrorSlot& error ) const
	{
		aCall<Dict>( connected_,
		             boost::bind( xmmsc_configval_list, conn_ ),
		             slot, error );
	}

	void
	Config::broadcastValueChanged( const DictSlot& slot,
	                               const ErrorSlot& error ) const
	{
		aCall<Dict>( connected_,
		             boost::bind( xmmsc_broadcast_configval_changed, conn_ ),
		             slot, error );
	}


	Config::Config( xmmsc_connection_t*& conn, bool& connected,
	                MainloopInterface*& ml ) :
		conn_( conn ), connected_( connected ), ml_( ml )
	{
	}

}
