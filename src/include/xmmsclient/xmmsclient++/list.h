#ifndef XMMSCLIENTPP_LIST_H
#define XMMSCLIENTPP_LIST_H

#include <xmmsclient/xmmsclient.h>
#include <boost/any.hpp>

#include <xmmsclient/xmmsclient++/dict.h>
#include <xmmsclient/xmmsclient++/typedefs.h>
#include <xmmsclient/xmmsclient++/exceptions.h>
#include <string>

namespace Xmms
{

	class SuperList
	{

		public:
			SuperList( xmmsc_result_t* result );
			SuperList( const SuperList& list );
			virtual SuperList& operator=( const SuperList& list );
			virtual ~SuperList();

			virtual void first();
			virtual void operator++();

			virtual bool isValid() const;

		protected:
			xmmsc_result_t* result_;
			bool constructed_;

			virtual void constructContents() = 0;

	};


	template< typename T >
	class List : public SuperList
	{

		public:
			List( xmmsc_result_t* result ) :
				SuperList( result ), contents_()
			{
			}

			List( const List<T>& list ) :
				SuperList( list ), contents_( list.contents_ )
			{
			}

			List<T>& operator=( const List<T>& list )
			{
				SuperList::operator=( list );
				contents_ = list.contents_;
				return *this;
			}

			virtual ~List()
			{
			}

			const T& operator*()
			{
				constructContents();
				return contents_;
			}

			const T& operator->()
			{
				constructContents();
				return contents_;
			}
			
		private:
			T contents_;

			virtual void constructContents() = 0;

	};

	template<>
	class List< int > : public SuperList
	{

		public:
			List( xmmsc_result_t* result ) :
				SuperList( result ), contents_( 0 )
			{

				if( xmmsc_result_get_type( result ) !=
				    XMMS_OBJECT_CMD_ARG_INT32 ) {
					// SuperList constructor refs the result so we'll unref
					xmmsc_result_unref( result );
					throw wrong_type_error( "Expected list of ints" );
				}

			}

			List( const List<int>& list ) :
				SuperList( list ), contents_( list.contents_ )
			{
			}

			List<int>& operator=( const List<int>& list )
			{   
				SuperList::operator=( list );
				contents_ = list.contents_;
				return *this;
			}

			virtual ~List()
			{
			}

			const int& operator*()
			{
				constructContents();
				return contents_;
			}

			const int& operator->()
			{
				constructContents();
				return contents_;
			}

		private:
			int contents_;

			virtual void constructContents()
			{

				if( constructed_ ) {
					return;
				}

				int temp = 0;
				if( !xmmsc_result_get_int( result_, &temp ) ) {
					// throw something
				}
				contents_ = temp;
				constructed_ = true;
				
			}

	};

	template<>
	class List< unsigned int > : public SuperList
	{

		public:
			List( xmmsc_result_t* result ) :
				SuperList( result ), contents_( 0 )
			{

				if( xmmsc_result_get_type( result ) !=
				    XMMS_OBJECT_CMD_ARG_UINT32 ) {
					// SuperList constructor refs the result so we'll unref
					xmmsc_result_unref( result );
					throw wrong_type_error( "Expected list of unsigned ints" );
				}

			}

			List( const List<unsigned int>& list ) :
				SuperList( list ), contents_( list.contents_ )
			{
			}

			List<unsigned int>& operator=( const List<unsigned int>& list )
			{
				SuperList::operator=( list );
				contents_ = list.contents_;
				return *this;
			}

			virtual ~List()
			{
			}

			const unsigned int& operator*()
			{
				constructContents();
				return contents_;
			}

			const unsigned int& operator->()
			{
				constructContents();
				return contents_;
			}

		private:
			unsigned int contents_;

			virtual void constructContents()
			{

				if( constructed_ ) {
					return;
				}

				unsigned int temp = 0;
				if( !xmmsc_result_get_uint( result_, &temp ) ) {
					// throw something
				}
				contents_ = temp;
				constructed_ = true;
				
			}

	};

	template<>
	class List< std::string > : public SuperList
	{

		public:
			List( xmmsc_result_t* result ) :
				SuperList( result ), contents_() 
			{

				if( xmmsc_result_get_type( result ) !=
				    XMMS_OBJECT_CMD_ARG_STRING ) {
					// SuperList constructor refs the result so we'll unref
					xmmsc_result_unref( result );
					throw wrong_type_error( "Expected list of strings" );
				}

			}

			List( const List<std::string>& list ) :
				SuperList( list ), contents_( list.contents_ )
			{
			}

			List<std::string>& operator=( const List<std::string>& list )
			{
				SuperList::operator=( list );
				contents_ = list.contents_;
				return *this;
			}

			virtual ~List()
			{
			}

			const std::string& operator*()
			{
				constructContents();
				return contents_;
			}

			const std::string& operator->()
			{
				constructContents();
				return contents_;
			}

		private:
			std::string contents_;

			virtual void constructContents()
			{

				if( constructed_ ) {
					return;
				}

				char* temp = 0;
				if( !xmmsc_result_get_string( result_, &temp ) ) {
					// throw something
				}
				contents_ = std::string( temp );
				constructed_ = true;
				
			}

	};

	template<>
	class List< Dict > : public SuperList
	{

		public:
			List( xmmsc_result_t* result ) try :
				SuperList( result ), contents_( result_ ) 
			{
				// checking the type here is a bit useless since
				// Dict constructor checks it but we must catch it and
				// unref the result which SuperList refs or we leak.
			}
			catch(...)
			{
				xmmsc_result_unref( result );
				throw;
			}

			List( const List<Dict>& list ) :
				SuperList( list ), contents_( list.contents_ )
			{
			}

			List<Dict>& operator=( const List<Dict>& list )
			{
				SuperList::operator=( list );
				contents_ = list.contents_;
				return *this;
			}

			virtual ~List()
			{
			}

			const Dict& operator*()
			{
				constructContents();
				return contents_;
			}

			const Dict* operator->()
			{
				constructContents();
				return &contents_;
			}

		private:
			Dict contents_;

			virtual void constructContents()
			{
				if( constructed_ ) {
					return;
				}

				contents_ = Dict( result_ );

				constructed_ = true;

			}

	};
}

#endif // XMMSCLIENTPP_LIST_H
