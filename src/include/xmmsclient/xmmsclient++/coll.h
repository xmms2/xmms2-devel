#ifndef XMMSCLIENTPP_COLL_H
#define XMMSCLIENTPP_COLL_H

#include <xmmsclient/xmmsclient.h>
#include <xmmsclient/xmmsclient++/typedefs.h>
#include <xmmsclient/xmmsclient++/exceptions.h>
#include <xmmsclient/xmmsclient++/coll.h>

#include <string>

namespace Xmms 
{

	class Coll;

	template< typename keyT, typename valT >
	class AbstractElement
	{
		public:
			virtual ~AbstractElement();

			// get value
			virtual operator valT() const = 0;

			// set value
			virtual valT operator=( valT value ) = 0;

		protected:
			AbstractElement( Coll& coll, keyT index );

			Coll& coll_;
			keyT index_;
	};

	/** @class Coll coll.h "xmmsclient/xmmsclient++/coll.h"
	 *  @brief This class is used to build collection structures.
	 */
	class Coll
	{

		class AttributeElement : public AbstractElement< std::string, std::string >
		{
			public:
				// get value
				operator std::string() const;

				// set value
				std::string operator=( std::string value );

			protected:
				friend class Coll;
				AttributeElement( Coll& coll, std::string index );
		};


	public: 
		class OperandIterator
		{

			public:
				OperandIterator( const OperandIterator& src );
				OperandIterator operator=( const OperandIterator& src ) const;
				~OperandIterator();

				bool first();
				bool valid();
				bool next();
				bool save();
				bool restore();

				Coll operator *();

			private:
				friend class Coll;
				OperandIterator( Coll& coll );

				Coll& coll_;
		};


		class Idlist
		{
			private:
				class IdlistElement;

			public:

				Idlist( const Idlist& src );
				Idlist operator=( const Idlist& src ) const;
				~Idlist();

				/* FIXME: or void and throw error ? */
				bool append( unsigned int id );
				bool insert( unsigned int id, unsigned int index );
				bool move( unsigned int index, unsigned int newindex );
				bool remove( unsigned int index );
				bool clear();

				unsigned int size() const;

				// get/set value at index
				IdlistElement operator []( unsigned int index );

			private:

				friend class Coll;
				Idlist( Coll& coll );

				Coll& coll_;


				class IdlistElement : public AbstractElement< unsigned int, unsigned int >
				{
					public:
						// get value
						operator unsigned int() const;

						// set value
						unsigned int operator=( unsigned int value );

					private:
						friend class Idlist;
						IdlistElement( Coll& coll, unsigned int index );
				};

		};





		public:

			typedef xmmsc_coll_type_t Type;

		/* FIXME: for testing */
			Coll( Type type );
			xmmsc_coll_t* coll_;

			class Universe;
			class Reference;
			class Union;
			class Intersection;
			class Complement;
			class Has;
			class Match;
			class Contains;
			class Smaller;
			class Greater;
			/* FIXME: name conflict: class Idlist; */
			class Queue;
			class PartyShuffle;
			/* ... to be implemented ... */

			/** Destructor.
			 */
			virtual ~Coll();

			void ref();
			void unref();

		// FIXME: support operator<< too ?
			void addOperand( Coll& operand );
			void removeOperand( Coll& operand );

			OperandIterator getOperandIterator();

			// get/set attributes
			AttributeElement operator []( const std::string& attrname );

		// FIXME: support operator[](unsigned int) ?
			Idlist getIdlist();

		/** @cond */
		private:

			// Constructor, prevent creation of Coll objects
		// FIXME: testing Coll( Type type );
			Coll( xmmsc_coll_t *coll );

			// Copy-constructor / operator=
			Coll( const Coll& src );
			Coll operator=( const Coll& src );

			void setAttribute( const std::string &attrname, const std::string &value );
			std::string getAttribute( const std::string &attrname );

			void idlistSetIndex( unsigned int index, unsigned int value );
			unsigned int idlistGetIndex( unsigned int index );

		// FIXME: testing xmmsc_coll_t* coll_;

		/** @endcond */

	};


	template< typename keyT, typename valT >
	AbstractElement< keyT, valT >::AbstractElement( Coll& coll, keyT index )
		: coll_ (coll), index_( index )
	{
		coll_.ref();
	}

	template< typename keyT, typename valT >
	AbstractElement< keyT, valT >::~AbstractElement()
	{
		coll_.unref();
	}


}

#endif // XMMSCLIENTPP_COLL_H
