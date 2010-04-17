/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2009 XMMS2 Team
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
	class CollResult;

	Coll::Coll* extract_collection( xmmsv_t* );

	/** @class Coll coll.h "xmmsclient/xmmsclient++/coll.h"
	 *  @brief This class is used to build collection structures.
	 */
	namespace Coll
	{

		typedef xmmsv_coll_type_t Type;

		const Type REFERENCE    = XMMS_COLLECTION_TYPE_REFERENCE;
		const Type UNION        = XMMS_COLLECTION_TYPE_UNION;
		const Type INTERSECTION = XMMS_COLLECTION_TYPE_INTERSECTION;
		const Type COMPLEMENT   = XMMS_COLLECTION_TYPE_COMPLEMENT;
		const Type HAS          = XMMS_COLLECTION_TYPE_HAS;
		const Type EQUALS       = XMMS_COLLECTION_TYPE_EQUALS;
		const Type MATCH        = XMMS_COLLECTION_TYPE_MATCH;
		const Type SMALLER      = XMMS_COLLECTION_TYPE_SMALLER;
		const Type GREATER      = XMMS_COLLECTION_TYPE_GREATER;
		const Type IDLIST       = XMMS_COLLECTION_TYPE_IDLIST;
		const Type QUEUE        = XMMS_COLLECTION_TYPE_QUEUE;
		const Type PARTYSHUFFLE = XMMS_COLLECTION_TYPE_PARTYSHUFFLE;

		class OperandIterator;
		class IdlistElement;
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

				virtual void append( int id );
				virtual void insert( unsigned int index, int id);
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
				xmmsv_coll_t* getColl() const { return coll_; }

			/** @cond */
			protected:

				friend class OperandIterator;
				friend class ::Xmms::Collection;
				friend class ::Xmms::CollResult;
				friend class IdlistElement;
				friend class Unary;

				Coll( xmmsv_coll_t *coll );
				Coll( Type type );
				Coll( const Coll& src );
				Coll operator=( const Coll& src );

				void setIndex( unsigned int index, int value );
				int getIndex( unsigned int index ) const;

				xmmsv_coll_t* coll_;

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
				Nary( xmmsv_coll_t* coll );
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
				Unary( xmmsv_coll_t* coll );
				~Unary();
		};

		// FIXME: support integer value too? depend on class?
		class Filter : public Unary
		{
			protected:
				Filter( xmmsv_coll_t* coll );
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


		class Reference : public Coll
		{
			friend Coll* ::Xmms::extract_collection( xmmsv_t* );
			//template<typename T> friend T* Xmms::extract_value( xmmsc_result_t* );
			friend class ::Xmms::Collection;
			friend class ::Xmms::CollResult;

			protected:
				Reference( xmmsv_coll_t* coll );

			public:
				// nsname is actually of type Collection::Namespace,
				// but we try to avoid a dependency hell here..
				Reference();
				Reference( const std::string& name,
				           const char* const& nsname );
				~Reference();
		};


        /** A collection operator corresponding to all the media in
		 *  the medialib.
		 *
		 *  Used attributes: none.
		 *
		 *  Operand: none.
		 *
		 *  A Universe operator is used to refer to the "All Media"
		 *  meta-collection that represents all the media in the
		 *  medialib.  Useful as input for other operators.
		 */
		class Universe : public Reference
		{
			public:
				Universe();
				~Universe();
		};


        /** A Union collection operator forms the union of multiple
		 *  operators.
		 *
		 *  Used attributes: none.
		 *
		 *  Operand: unbounded.
		 *
		 *  The collection produced by a Union operator is the union
		 *  of all its operands, i.e. all the media matching
		 *  <em>any</em> of the collection operands.
		 */
		class Union : public Nary
		{
			friend class ::Xmms::Collection;
			friend class ::Xmms::CollResult;
			friend Coll* ::Xmms::extract_collection( xmmsv_t* );

			protected:
				Union( xmmsv_coll_t* coll );

			public:
				Union();
				~Union();
		};


        /** An Intersection collection operator forms the intersection
		 *  of multiple operators.
		 *
		 *  Used attributes: none.
		 *
		 *  Operand: unbounded.
		 *
		 *  The collection produced by an Intersection operator is the
		 *  intersection of all its operands, i.e. all the media
		 *  matching <em>all</em> the collection operands.
		 */
		class Intersection : public Nary
		{
			friend class ::Xmms::Collection;
			friend class ::Xmms::CollResult;
			friend Coll* ::Xmms::extract_collection( xmmsv_t* );

			protected:
				Intersection( xmmsv_coll_t* coll );

			public:
				Intersection();
				~Intersection();
		};


        /** A Complement collection operator forms the complement of
		 *  an operators.
		 *
		 *  Used attributes: none.
		 *
		 *  Operand: 1.
		 *
		 *  The collection produced by an Intersection operator is the
		 *  complement of its unique operand, i.e. all the media
		 *  <em>not</em> matching the collection operand.
		 */
		class Complement : public Unary
		{
			friend class ::Xmms::Collection;
			friend class ::Xmms::CollResult;
			friend Coll* ::Xmms::extract_collection( xmmsv_t* );

			protected:
				Complement( xmmsv_coll_t* coll );

			public:
				Complement();
				Complement( Coll& operand );
				~Complement();
		};

		class Has : public Filter
		{
			friend class ::Xmms::Collection;
			friend class ::Xmms::CollResult;
			friend Coll* ::Xmms::extract_collection( xmmsv_t* );
			//template<typename T> friend T* Xmms::extract_value( xmmsc_result_t* );

			protected:
				Has( xmmsv_coll_t* coll );

			public:
				Has();
				Has(Coll& operand);
				Has(Coll& operand, const std::string& field);
				~Has();
		};

		class Smaller : public Filter
		{
			friend class ::Xmms::Collection;
			friend class ::Xmms::CollResult;
			friend Coll* ::Xmms::extract_collection( xmmsv_t* );
			//template<typename T> friend T* Xmms::extract_value( xmmsc_result_t* );

			protected:
				Smaller( xmmsv_coll_t* coll );

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
			friend class ::Xmms::CollResult;
			friend Coll* ::Xmms::extract_collection( xmmsv_t* );
			//template<typename T> friend T* Xmms::extract_value( xmmsc_result_t* );

			protected:
				Greater( xmmsv_coll_t* coll );

			public:
				Greater();
				Greater(Coll& operand);
				Greater(Coll& operand, const std::string& field);
				Greater(Coll& operand,
				        const std::string& field,
				        const std::string& value);
				~Greater();
		};

		class Equals : public Filter
		{
			friend class ::Xmms::Collection;
			friend class ::Xmms::CollResult;
			friend Coll* ::Xmms::extract_collection( xmmsv_t* );
			//template<typename T> friend T* Xmms::extract_value( xmmsc_result_t* );

			protected:
				Equals( xmmsv_coll_t* coll );

			public:
				Equals();
				Equals(Coll& operand);
				Equals(Coll& operand, const std::string& field);
				Equals(Coll& operand,
				      const std::string& field,
				      const std::string& value,
				      bool case_sensitive = false);
				~Equals();
		};

		class Match : public Filter
		{
			friend class ::Xmms::Collection;
			friend class ::Xmms::CollResult;
			friend Coll* ::Xmms::extract_collection( xmmsv_t* );
			//template<typename T> friend T* Xmms::extract_value( xmmsc_result_t* );

			protected:
				Match( xmmsv_coll_t* coll );

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


        /** An Idlist collection operator.
		 *
		 *  Used attributes: none.
		 *
		 *  Operand: none.
		 *
		 *  The Idlist operator stores a fixed list of media id.  The
		 *  list is ordered, and that ordering is used if the operator
		 *  is used as a playlist.
		 *
		 *  The Idlist operator also unmasks the methods related to
		 *  managing the internal id-list, as well as the [] bracket
		 *  operator with integer argument for direct access.
		 */
		class Idlist : public Coll
		{
			friend class Element;
			friend class ::Xmms::Collection;
			friend class ::Xmms::CollResult;
			friend Coll* ::Xmms::extract_collection( xmmsv_t* );

			protected:
				Idlist( xmmsv_coll_t* coll );
				Idlist( Type type );

			public:
				Idlist();
				~Idlist();

				void append( int id );
				void insert( unsigned int index, int id);
				void move( unsigned int index, unsigned int newindex );
				void remove( unsigned int index );
				void clear();

				unsigned int size() const;

				// get/set value at index
				IdlistElement operator []( unsigned int index );
				const IdlistElement operator []( unsigned int index ) const;
		};


        /** A Queue collection operator.
		 *
		 *  Used attributes:
		 *  - history: if used as a playlist, determines how many
		 *             played items remain before they are popped
		 *             off the queue.
		 *
		 *  Operand: none.
		 *
		 *  The Queue operator is similar to the Idlist operator,
		 *  except if loaded as a playlist, only <em>history</em>
		 *  played items will remain and the previous ones are
		 *  removed.
		 */
		class Queue : public Idlist
		{
			friend class ::Xmms::Collection;
			friend class ::Xmms::CollResult;
			friend Coll* ::Xmms::extract_collection( xmmsv_t* );
			//template<typename T> friend T* Xmms::extract_value( xmmsc_result_t* );

			protected:
				Queue( xmmsv_coll_t* coll );
				Queue( Type type );
				Queue( Type type, unsigned int history );

			public:
				Queue();
				Queue( unsigned int history );
				~Queue();
		};


        /** A PartyShuffle collection operator.
		 *
		 *  Used attributes:
		 *  - history: if used as a playlist, determines how many
		 *             played items remain before they are popped
		 *             off the queue.
		 *  - upcoming: if used as a playlist, determines the minimum
		 *              number of incoming entries (if fewer, new entries
		 *              are randomly fetched from the operand collection).
		 * - jumplist: optionally, the name of a playlist to jump to
         *             once the PartyShuffle is done playing.
		 *
		 *  Operand: 1, the input collection to randomly fetch media from.
		 *
		 *  The PartyShuffle operator is similar to the Queue operator
		 *  (entries are popped if they exceed <em>history</em>), but
		 *  when loaded, the playlist is automatically fed by random
		 *  media taken from the input collection until its size reaches
		 */
		class PartyShuffle : public Queue
		{
			friend class ::Xmms::Collection;
			friend class ::Xmms::CollResult;
			friend Coll* ::Xmms::extract_collection( xmmsv_t* );

			protected:
				PartyShuffle( xmmsv_coll_t* coll );

			public:
				PartyShuffle();
				PartyShuffle( unsigned int history );
				PartyShuffle( unsigned int history, unsigned int upcoming );
				~PartyShuffle();

				void setOperand( Coll& operand );
				void removeOperand();
				CollPtr getOperand() const;
		};



		class OperandIterator
		{

			public:
				OperandIterator( const OperandIterator& src );
				OperandIterator operator=( const OperandIterator& src );
				~OperandIterator();

				void first();
				bool valid() const;
				void next();

				CollPtr operator *() const;
				// FIXME: Operator -> ?

			private:

				friend class Nary;
				OperandIterator( Coll& coll );
				OperandIterator( const Coll& coll );

				void initIterator();

				Coll& coll_;
				xmmsv_list_iter_t *oper_it_;
		};


		/** Helper class accessing one element of an Idlist operator
		 *  for reading or writing.
		 *
		 *  This class should not be instanciated by users of the library.
		 */
		class IdlistElement : public AbstractElement< unsigned int, int >
		{
			public:
				~IdlistElement();
				operator int() const;
				int operator=( int value );

			private:
				friend class Idlist;
				IdlistElement( Coll& coll, unsigned int index );
				IdlistElement( const Coll& coll, unsigned int index );
		};



		template< typename keyT, typename valT >
		AbstractElement< keyT, valT >::AbstractElement( Coll& coll, keyT index )
			: coll_ (coll), index_( index )
		{
			xmmsv_coll_ref( coll_.getColl() );
		}

		template< typename keyT, typename valT >
		AbstractElement< keyT, valT >::AbstractElement( const Coll& coll, keyT index )
			: coll_ (coll), index_( index )
		{
			xmmsv_coll_ref( coll_.getColl() );
		}

		template< typename keyT, typename valT >
		AbstractElement< keyT, valT >::~AbstractElement()
		{
			xmmsv_coll_unref( coll_.getColl() );
		}

	}

}

#endif // XMMSCLIENTPP_COLL_H
