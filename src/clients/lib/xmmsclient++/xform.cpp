#include <xmmsclient/xmmsclient.h>
#include <xmmsclient/xmmsclient++/medialib.h>
#include <xmmsclient/xmmsclient++/mainloop.h>
#include <xmmsclient/xmmsclient++/dict.h>
#include <xmmsclient/xmmsclient++/typedefs.h>
#include <xmmsclient/xmmsclient++/helpers.h>
#include <xmmsclient/xmmsclient++/list.h>
#include <xmmsclient/xmmsclient++/xform.h>

#include <boost/function.hpp>
#include <boost/bind.hpp>

#include <string>

namespace Xmms
{

	Xform::~Xform()
	{
	}

	List< Dict >
	Xform::browse( const std::string& url ) const
	{

		xmmsc_result_t* res =
		    call( connected_, ml_,
		          boost::bind( xmmsc_xform_media_browse, conn_, url.c_str() )
		        );

		List< Dict > result( res );
		xmmsc_result_unref( res );

		return res;
	}

	void
	Xform::browse( const std::string& url, const DictListSlot& slot,
				   const ErrorSlot& error ) const
	{

		aCall<DictList>( connected_, 
							 boost::bind( xmmsc_xform_media_browse, conn_,
										  url.c_str() ),
							 slot, error );

	}

	Xform::Xform( xmmsc_connection_t*& conn, bool& connected,
				  MainloopInterface*& ml ) :
		conn_( conn ), connected_( connected ), ml_( ml )
	{
	}

}
