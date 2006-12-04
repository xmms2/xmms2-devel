#include <xmmsclient/xmmsclient.h>
#include <xmmsclient/xmmsclient++/bindata.h>
#include <xmmsclient/xmmsclient++/mainloop.h>
#include <xmmsclient/xmmsclient++/helpers.h>
#include <xmmsclient/xmmsclient++/typedefs.h>

#include <boost/bind.hpp>

#include <string>

namespace Xmms
{

	Bindata::~Bindata()
	{
	}

	std::string Bindata::add( const Xmms::bin& data ) const
	{
		xmmsc_result_t* res =
			call( connected_, ml_,
			      boost::bind( xmmsc_bindata_add, conn_,
			                   data.data(), data.size() ) );

		char* temp = 0;
		xmmsc_result_get_string( res, &temp );

		std::string result( temp );
		xmmsc_result_unref( res );

		return result;
	}

	Xmms::bin Bindata::retrieve( const std::string& hash ) const
	{
		xmmsc_result_t* res =
			call( connected_, ml_,
			      boost::bind( xmmsc_bindata_retrieve, conn_, hash.c_str() ) );

		unsigned char* temp = 0;
		unsigned int len = 0;
		xmmsc_result_get_bin( res, &temp, &len );

		Xmms::bin result( temp, len );
		xmmsc_result_unref( res );

		return result;
	}

	void Bindata::remove( const std::string& hash ) const
	{
		vCall( connected_, ml_,
		       boost::bind( xmmsc_bindata_remove, conn_, hash.c_str() ) );
	}

	void Bindata::add( const Xmms::bin& data, const StringSlot& slot,
	                   const ErrorSlot& error ) const
	{
		aCall<std::string>( connected_,
		                    boost::bind( xmmsc_bindata_add, conn_,
		                                 data.data(), data.size() ),
		                    slot, error );
	}

	void Bindata::retrieve( const std::string& hash, const BinSlot& slot, 
	                        const ErrorSlot& error ) const
	{
		aCall<Xmms::bin>( connected_,
		                  boost::bind( xmmsc_bindata_retrieve, conn_,
		                               hash.c_str() ),
		                  slot, error );
	}

	void Bindata::remove( const std::string& hash, const VoidSlot& slot,
	                      const ErrorSlot& error ) const
	{
		aCall<void>( connected_,
		             boost::bind( xmmsc_bindata_remove, conn_, hash.c_str() ),
		             slot, error );
	}


	Bindata::Bindata( xmmsc_connection_t*& conn, bool& connected,
	                  MainloopInterface*& ml ) :
		conn_( conn ), connected_( connected ), ml_( ml )
	{
	}

}
