#include <xmmsclient/xmmsclient.h>

#include <xmmsclient/xmmsclient++/list.h>
#include <xmmsclient/xmmsclient++/exceptions.h>

#include <boost/any.hpp>

#include <string>

namespace Xmms
{

	Detail::SuperList::SuperList( xmmsc_result_t* result )
		: result_( 0 ), constructed_( false )
	{

		if( xmmsc_result_iserror( result ) ) {
			throw result_error( xmmsc_result_get_error( result ) );
		}
		if( !xmmsc_result_is_list( result ) ) {
			throw not_list_error( "Provided result is not a list" );
		}

		result_ = result;
		xmmsc_result_ref( result_ );

	}

	Detail::SuperList::SuperList( const SuperList& list )
		: result_( list.result_ ), constructed_( list.constructed_ )
	{
		xmmsc_result_ref( result_ );
	}

	Detail::SuperList& Detail::SuperList::operator=( const SuperList& list )
	{
		result_ = list.result_;
		constructed_ = list.constructed_;
		xmmsc_result_ref( result_ );
		return *this;
	}

	Detail::SuperList::~SuperList()
	{
		xmmsc_result_unref( result_ );
	}

	void Detail::SuperList::first()
	{

		if( !xmmsc_result_list_first( result_ ) ) {
			// throw something...
		}
		constructed_ = false;

	}

	void Detail::SuperList::operator++()
	{
		if( !xmmsc_result_list_next( result_ ) ) {
			// throw
		}
		constructed_ = false;
	}

	bool Detail::SuperList::isValid() const
	{
		return xmmsc_result_list_valid( result_ );
	}

}
