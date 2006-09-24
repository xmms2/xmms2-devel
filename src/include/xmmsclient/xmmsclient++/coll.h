#ifndef XMMSCLIENTPP_COLL_H
#define XMMSCLIENTPP_COLL_H

#include <xmmsclient/xmmsclient.h>
#include <xmmsclient/xmmsclient++/typedefs.h>
#include <xmmsclient/xmmsclient++/exceptions.h>
#include <xmmsclient/xmmsclient++/collection.h>

#include <string>
#include <iostream>
#include <stdexcept>

namespace Xmms 
{

	/** @class Coll coll.h "xmmsclient/xmmsclient++/coll.h"
	 *  @brief This class is used to build collection structures.
	 */
	namespace Coll
	{

		typedef xmmsc_coll_type_t Type;

		class OperandIterator;
		class Coll;

		// FIXME: Hide classes?

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

				// to avoid problems with ostream and implicit casting operator
				friend std::ostream& operator<<( std::ostream& os,
				                                 const AbstractElement< keyT, valT >& elem )
				{
					os << keyT( elem );
					return os;
				}

				Coll& coll_;
				keyT index_;
		};

		class AttributeElement : public AbstractElement< std::string, std::string >
		{
			public:
				~AttributeElement();
				operator std::string() const;
				std::string operator=( std::string value );

			protected:
				friend class Coll;
				AttributeElement( Coll& coll, std::string index );
		};


		class Coll
		{
			public:

				/* FIXME: for testing */
				xmmsc_coll_t* coll_;

				/** Destructor.
				 */
				virtual ~Coll();

				void ref();
				void unref();

				// get/set attributes
				AttributeElement operator []( const std::string& attrname );

				void setAttribute( const std::string &attrname, const std::string &value );
				std::string getAttribute( const std::string &attrname ) const;
				void removeAttribute( const std::string &attrname );

				// FIXME: Make these protected!
				void setIndex( unsigned int index, unsigned int value );
				unsigned int getIndex( unsigned int index ) const;

			/** @cond */
			protected:

				// FIXME: testing xmmsc_coll_t* coll_;
				friend class OperandIterator;
				friend class Unary;            // FIXME: why do we need that??
				Coll( xmmsc_coll_t *coll );
				Coll( Type type );
				Coll( const Coll& src );
				Coll operator=( const Coll& src );

			/** @endcond */
		};


		class Nary : public Coll
		{
			public:
				// FIXME: support operator<< too ?
				void addOperand( Coll& operand );
				void removeOperand( Coll& operand );

				OperandIterator getOperandIterator();

			protected:
				Nary( Type type );
				~Nary();
		};

		class Unary : public Coll
		{
			public:
				void setOperand( Coll& operand );
				void removeOperand();
				Coll getOperand() const;

			protected:
				Unary( Type type );
				Unary( Type type, Coll& operand );
				~Unary();
		};

		// FIXME: support integer value too? depend on class?
		class Filter : public Unary
		{
			protected:
				Filter( Type type );
				Filter( Type type, Coll& operand );
				Filter( Type type, Coll& operand, const std::string& field);
				Filter( Type type,
				        Coll& operand,
				        const std::string& field,
				        const std::string& value );
				Filter( Type type,
				        Coll& operand,
				        const std::string& field,
				        const std::string& value,
				        bool case_sensitive );
				~Filter();
		};


		class OperandIterator
		{

			public:
				OperandIterator( const OperandIterator& src );
				OperandIterator operator=( const OperandIterator& src ) const;
				~OperandIterator();

				void first();
				bool valid() const;
				void next();
				void save();
				void restore();

				Coll operator *() const;
				// FIXME: Operator -> ?

			private:

				friend class Nary;
				OperandIterator( Coll& coll );

				Coll& coll_;
		};


		class Reference : public Coll
		{
			public:
				Reference();
				Reference( const std::string& name,
				           const Collection::Namespace& nsname );
				~Reference();
		};

		class Universe : public Reference
		{
			public:
				Universe();
				~Universe();
		};

		class Union : public Nary
		{
			public:
				Union();
				~Union();
		};

		class Intersection : public Nary
		{
			public:
				Intersection();
				~Intersection();
		};

		class Complement : public Unary
		{
			public:
				Complement();
				Complement( Coll& operand );
				~Complement();
		};

		class Has : public Filter
		{
			public:
				Has();
				Has(Coll& operand);
				Has(Coll& operand, const std::string& field);
				~Has();
		};

		class Smaller : public Filter
		{
			public:
				Smaller();
				Smaller(Coll& operand);
				Smaller(Coll& operand, const std::string& field);
				Smaller(Coll& operand,
				        const std::string& field,
				        const std::string& value);
				~Smaller();
		};

		class Greater : public Filter
		{
			public:
				Greater();
				Greater(Coll& operand);
				Greater(Coll& operand, const std::string& field);
				Greater(Coll& operand,
				        const std::string& field,
				        const std::string& value);
				~Greater();
		};

		class Match : public Filter
		{
			public:
				Match();
				Match(Coll& operand);
				Match(Coll& operand, const std::string& field);
				Match(Coll& operand,
				      const std::string& field,
				      const std::string& value,
				      bool case_sensitive = false);
				~Match();
		};

		class Contains : public Filter
		{
			public:
				Contains();
				Contains(Coll& operand);
				Contains(Coll& operand, const std::string& field);
				Contains(Coll& operand,
				         const std::string& field,
				         const std::string& value,
				         bool case_sensitive = false);
				~Contains();
		};

		class Idlist : public Coll
		{
			friend class Element;

			class Element : public AbstractElement< unsigned int, unsigned int >
			{
				public:
					~Element();
					operator unsigned int() const;
					unsigned int operator=( unsigned int value );

				private:
					friend class Idlist;
					Element( Coll& coll, unsigned int index );
			};


			public:
				Idlist();
				~Idlist();

				void append( unsigned int id );
				void insert( unsigned int id, unsigned int index );
				void move( unsigned int index, unsigned int newindex );
				void remove( unsigned int index );
				void clear();

				unsigned int size() const;

				// get/set value at index
				Element operator []( unsigned int index );
		};

		class Queue : public Idlist
		{
			public:
				Queue();
				Queue(unsigned int history);
				~Queue();
		};

		class PartyShuffle : public Queue
		{
			public:
				PartyShuffle();
				PartyShuffle(unsigned int history);
				PartyShuffle(unsigned int history, unsigned int upcoming);
				~PartyShuffle();
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

}

#endif // XMMSCLIENTPP_COLL_H
