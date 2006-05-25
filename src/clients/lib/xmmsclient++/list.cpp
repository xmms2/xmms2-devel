#include <xmmsclient/xmmsclient.h>

#include <xmmsclient/xmmsclient++/list.h>
#include <xmmsclient/xmmsclient++/exceptions.h>

#include <boost/any.hpp>

#include <string>

namespace Xmms
{

	SuperList::SuperList( xmmsc_result_t* result )
		: result_( 0 )
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

	SuperList::SuperList( const SuperList& list )
		: result_( list.result_ )
	{
		xmmsc_result_ref( result_ );
	}

	SuperList& SuperList::operator=( const SuperList& list )
	{
		result_ = list.result_;
		xmmsc_result_ref( result_ );
		return *this;
	}

	SuperList::~SuperList()
	{
		xmmsc_result_unref( result_ );
	}

	void SuperList::first() const
	{

		if( !xmmsc_result_list_first( result_ ) ) {
			// throw something...
		}

	}

	void SuperList::operator++() const
	{
		if( !xmmsc_result_list_next( result_ ) ) {
			// throw
		}
	}

	bool SuperList::isValid() const
	{
		return xmmsc_result_list_valid( result_ );
	}

}
