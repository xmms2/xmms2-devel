/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2006 XMMS2 Team
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

#ifndef XMMSCLIENTPP_COLL_H
#define XMMSCLIENTPP_COLL_H

#include <xmmsclient/xmmsclient.h>
#include <xmmsclient/xmmsclient++/typedefs.h>

#include <string>
#include <iostream>
#include <stdexcept>

namespace Xmms 
{

	class Collection;

	Coll::Coll* extract_collection( xmmsc_result_t* );

	/** @class Coll coll.h "xmmsclient/xmmsclient++/coll.h"
	 *  @brief This class is used to build collection structures.
	 */
	namespace Coll
	{

		typedef xmmsc_coll_type_t Type;

		const Type ERROR        = XMMS_COLLECTION_TYPE_ERROR;
		const Type REFERENCE    = XMMS_COLLECTION_TYPE_REFERENCE;
		const Type UNION        = XMMS_COLLECTION_TYPE_UNION;
		const Type INTERSECTION = XMMS_COLLECTION_TYPE_INTERSECTION;
		const Type COMPLEMENT   = XMMS_COLLECTION_TYPE_COMPLEMENT;
		const Type HAS          = XMMS_COLLECTION_TYPE_HAS;
		const Type MATCH        = XMMS_COLLECTION_TYPE_MATCH;
		const Type CONTAINS     = XMMS_COLLECTION_TYPE_CONTAINS;
		const Type SMALLER      = XMMS_COLLECTION_TYPE_SMALLER;
		const Type GREATER      = XMMS_COLLECTION_TYPE_GREATER;
		const Type IDLIST       = XMMS_COLLECTION_TYPE_IDLIST;
		const Type QUEUE        = XMMS_COLLECTION_TYPE_QUEUE;
		const Type PARTYSHUFFLE = XMMS_COLLECTION_TYPE_PARTYSHUFFLE;

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
				AbstractElement( const Coll& coll, keyT index );

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
				AttributeElement( const Coll& coll, std::string index );
		};

		class IdlistElement;
		class Coll
		{
			public:

				/** Destructor.
				 */
				virtual ~Coll();

				Type getType() const;

				// get/set attributes
				AttributeElement operator []( const std::string& attrname );
				const AttributeElement operator []( const std::string& attrname ) const;

				void setAttribute( const std::string &attrname, const std::string &value );
				std::string getAttribute( const std::string &attrname ) const;
				void removeAttribute( const std::string &attrname );

				virtual void addOperand( Coll& operand );
				virtual void removeOperand( Coll& operand );

				virtual void removeOperand();
				virtual void setOperand( Coll& operand );
				virtual CollPtr getOperand() const;

				virtual void append( unsigned int id );
				virtual void insert( unsigned int id, unsigned int index );
				virtual void move( unsigned int index,
				                   unsigned int newindex );
				virtual void remove( unsigned int index );
				virtual void clear();

				virtual unsigned int size() const;

				virtual IdlistElement operator []( unsigned int index );
				virtual const IdlistElement operator []( unsigned int index ) const;

				virtual OperandIterator getOperandIterator();
				virtual const OperandIterator getOperandIterator() const;

				// FIXME: Hide this, we shouldn't need it..
				xmmsc_coll_t* getColl() const { return coll_; }

			/** @cond */
			protected:

				friend class OperandIterator;
				friend class ::Xmms::Collection;
				friend class IdlistElement;
				friend class Unary;

				Coll( xmmsc_coll_t *coll );
				Coll( Type type );
				Coll( const Coll& src );
				Coll operator=( const Coll& src );

				void setIndex( unsigned int index, unsigned int value );
				unsigned int getIndex( unsigned int index ) const;

				xmmsc_coll_t* coll_;

				void ref();
				void unref();


			/** @endcond */
		};


		class Nary : public Coll
		{
			public:
				// FIXME: support operator<< too ?
				void addOperand( Coll& operand );
				void removeOperand( Coll& operand );

				OperandIterator getOperandIterator();
				const OperandIterator getOperandIterator() const;

			protected:
				Nary( Type type );
				Nary( xmmsc_coll_t* coll );
				~Nary();
		};

		class Unary : public Coll
		{
			public:
				void setOperand( Coll& operand );
				void removeOperand();
				CollPtr getOperand() const;

			protected:
				Unary( Type type );
				Unary( Type type, Coll& operand );
				Unary( xmmsc_coll_t* coll );
				~Unary();
		};

		// FIXME: support integer value too? depend on class?
		class Filter : public Unary
		{
			protected:
				Filter( xmmsc_coll_t* coll );
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

				CollPtr operator *() const;
				// FIXME: Operator -> ?

			private:

				friend class Nary;
				OperandIterator( Coll& coll );
				OperandIterator( const Coll& coll );

				Coll& coll_;
		};


		class Reference : public Coll
		{
			friend Coll* ::Xmms::extract_collection( xmmsc_result_t* );
			//template<typename T> friend T* Xmms::extract_value( xmmsc_result_t* );
			friend class ::Xmms::Collection;

			protected:
				Reference( xmmsc_coll_t* coll );

			public:
				// nsname is actually of type Collection::Namespace,
				// but we try to avoid a dependency hell here..
				Reference();
				Reference( const std::string& name,
				           const char* const& nsname );
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
			friend class ::Xmms::Collection;
			friend Coll* ::Xmms::extract_collection( xmmsc_result_t* );
			//template<typename T> friend T* Xmms::extract_value( xmmsc_result_t* );

			protected:
				Union( xmmsc_coll_t* coll );

			public:
				Union();
				~Union();
		};

		class Intersection : public Nary
		{
			friend class ::Xmms::Collection;
			friend Coll* ::Xmms::extract_collection( xmmsc_result_t* );
			//template<typename T> friend T* Xmms::extract_value( xmmsc_result_t* );

			protected:
				Intersection( xmmsc_coll_t* coll );

			public:
				Intersection();
				~Intersection();
		};

		class Complement : public Unary
		{
			friend class ::Xmms::Collection;
			friend Coll* ::Xmms::extract_collection( xmmsc_result_t* );
			//template<typename T> friend T* Xmms::extract_value( xmmsc_result_t* );

			protected:
				Complement( xmmsc_coll_t* coll );

			public:
				Complement();
				Complement( Coll& operand );
				~Complement();
		};

		class Has : public Filter
		{
			friend class ::Xmms::Collection;
			friend Coll* ::Xmms::extract_collection( xmmsc_result_t* );
			//template<typename T> friend T* Xmms::extract_value( xmmsc_result_t* );

			protected:
				Has( xmmsc_coll_t* coll );

			public:
				Has();
				Has(Coll& operand);
				Has(Coll& operand, const std::string& field);
				~Has();
		};

		class Smaller : public Filter
		{
			friend class ::Xmms::Collection;
			friend Coll* ::Xmms::extract_collection( xmmsc_result_t* );
			//template<typename T> friend T* Xmms::extract_value( xmmsc_result_t* );

			protected:
				Smaller( xmmsc_coll_t* coll );

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
			friend class ::Xmms::Collection;
			friend Coll* ::Xmms::extract_collection( xmmsc_result_t* );
			//template<typename T> friend T* Xmms::extract_value( xmmsc_result_t* );

			protected:
				Greater( xmmsc_coll_t* coll );

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
			friend class ::Xmms::Collection;
			friend Coll* ::Xmms::extract_collection( xmmsc_result_t* );
			//template<typename T> friend T* Xmms::extract_value( xmmsc_result_t* );

			protected:
				Match( xmmsc_coll_t* coll );

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
			friend class ::Xmms::Collection;
			friend Coll* ::Xmms::extract_collection( xmmsc_result_t* );
			//template<typename T> friend T* Xmms::extract_value( xmmsc_result_t* );

			protected:
				Contains( xmmsc_coll_t* coll );

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
			friend class ::Xmms::Collection;
			friend Coll* ::Xmms::extract_collection( xmmsc_result_t* );
			//template<typename T> friend T* Xmms::extract_value( xmmsc_result_t* );

			protected:
				Idlist( xmmsc_coll_t* coll );
				Idlist( Type type );

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
				IdlistElement operator []( unsigned int index );
				const IdlistElement operator []( unsigned int index ) const;
		};

		class IdlistElement : public AbstractElement< unsigned int, unsigned int >
		{
			public:
				~IdlistElement();
				operator unsigned int() const;
				unsigned int operator=( unsigned int value );

			private:
				friend class Idlist;
				IdlistElement( Coll& coll, unsigned int index );
				IdlistElement( const Coll& coll, unsigned int index );
		};

		class Queue : public Idlist
		{
			friend class ::Xmms::Collection;
			friend Coll* ::Xmms::extract_collection( xmmsc_result_t* );
			//template<typename T> friend T* Xmms::extract_value( xmmsc_result_t* );

			protected:
				Queue( xmmsc_coll_t* coll );
				Queue( Type type );
				Queue( Type type, unsigned int history );

			public:
				Queue();
				Queue( unsigned int history );
				~Queue();
		};

		class PartyShuffle : public Queue
		{
			friend class ::Xmms::Collection;
			friend Coll* ::Xmms::extract_collection( xmmsc_result_t* );
			//template<typename T> friend T* Xmms::extract_value( xmmsc_result_t* );

			protected:
				PartyShuffle( xmmsc_coll_t* coll );

			public:
				PartyShuffle();
				PartyShuffle( unsigned int history );
				PartyShuffle( unsigned int history, unsigned int upcoming );
				~PartyShuffle();
		};



		template< typename keyT, typename valT >
		AbstractElement< keyT, valT >::AbstractElement( Coll& coll, keyT index )
			: coll_ (coll), index_( index )
		{
			//coll_.ref();
			xmmsc_coll_ref( coll_.getColl() );
		}

		template< typename keyT, typename valT >
		AbstractElement< keyT, valT >::AbstractElement( const Coll& coll, keyT index )
			: coll_ (coll), index_( index )
		{
			//coll_.ref();
			xmmsc_coll_ref( coll_.getColl() );
		}

		template< typename keyT, typename valT >
		AbstractElement< keyT, valT >::~AbstractElement()
		{
			//coll_.unref();
			xmmsc_coll_unref( coll_.getColl() );
		}

	}

}

#endif // XMMSCLIENTPP_COLL_H
