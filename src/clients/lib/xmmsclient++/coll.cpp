/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2016 XMMS2 Team
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

#include <xmmsclient/xmmsclient.h>
#include <xmmsclient/xmmsclient++/client.h>
#include <xmmsclient/xmmsclient++/coll.h>
#include <xmmsclient/xmmsclient++/result.h>

#include <string>
using std::string;

#include <sstream>
#include <stdexcept>
#include <boost/lexical_cast.hpp>

namespace Xmms
{

	namespace Coll
	{

	Coll::Coll( Type type )
	{
		coll_ = xmmsv_new_coll( type );
		if( !coll_ ) {
			throw std::runtime_error( "Failed to create a Coll object" );
		}
	}

	Coll::Coll( xmmsv_t *coll )
		: coll_( coll )
	{
		ref();
	}

	Coll::Coll( const Coll& src )
		: coll_( src.coll_ )
	{
		ref();
	}

	Coll Coll::operator=( const Coll& src )
	{
		unref();
		coll_ = src.coll_;
		ref();
		return *this;
	}

	Coll::~Coll()
	{
		unref();
	}

	void Coll::ref()
	{
		xmmsv_ref( coll_ );
	}

	void Coll::unref()
	{
		xmmsv_unref( coll_ );
	}

	Type Coll::getType() const {
		return xmmsv_coll_get_type( coll_ );
	}

	AttributeElement Coll::operator []( const string& attrname )
	{
		return AttributeElement( *this, attrname );
	}

	const AttributeElement Coll::operator []( const string& attrname ) const
	{
		return AttributeElement( *this, attrname );
	}

	void Coll::setAttribute( const string &attrname, const string &value )
	{
		xmmsv_coll_attribute_set_string( coll_, attrname.c_str(), value.c_str() );
	}

	string Coll::getAttribute( const string &attrname ) const
	{
		const char *val;
		if( !xmmsv_coll_attribute_get_string( coll_, attrname.c_str(), &val ) ) {
			throw no_such_key_error( "No such attribute: " + attrname );
		}

		return string( val );
	}

	void Coll::removeAttribute( const string &attrname )
	{
		if( !xmmsv_coll_attribute_remove( coll_, attrname.c_str() ) ) {
			throw no_such_key_error( "No such attribute: " + attrname );
		}
	}

	void Coll::setIndex( unsigned int index, int value )
	{
		if( !xmmsv_coll_idlist_set_index( coll_, index, value ) ) {
			std::stringstream err;
			err << "Index out of idlist: "  << index;
			throw out_of_range( err.str() );
		}
	}

	int Coll::getIndex( unsigned int index ) const
	{
		int value;
		if( !xmmsv_coll_idlist_get_index( coll_, index, &value ) ) {
			std::stringstream err;
			err << "Index out of idlist: "  << index;
			throw out_of_range( err.str() );
		}

		return value;
	}

	void Coll::addOperand( Coll& ) {
		throw collection_type_error( "Wrong type" );
	}

	void Coll::removeOperand( Coll& ) {
		throw collection_type_error( "Wrong type" );
	}

	void Coll::removeOperand() {
		throw collection_type_error( "Wrong type" );
	}

	void Coll::setOperand( Coll& ) {
		throw collection_type_error( "Wrong type" );
	}

	CollPtr Coll::getOperand() const {
		throw collection_type_error( "Wrong type" );
	}

	void Coll::append( int ) {
		throw collection_type_error( "Wrong type" );
	}

	void Coll::insert( unsigned int, int ) {
		throw collection_type_error( "Wrong type" );
	}

	void Coll::move( unsigned int, unsigned int ) {
		throw collection_type_error( "Wrong type" );
	}

	void Coll::remove( unsigned int ) {
		throw collection_type_error( "Wrong type" );
	}

	void Coll::clear() {
		throw collection_type_error( "Wrong type" );
	}

	unsigned int Coll::size() const {
		throw collection_type_error( "Wrong type" );
	}

	OperandIterator Coll::getOperandIterator() {
		throw collection_type_error( "Wrong type" );
	}

	const OperandIterator Coll::getOperandIterator() const {
		throw collection_type_error( "Wrong type" );
	}

	IdlistElement Coll::operator []( unsigned int ) {
		throw collection_type_error( "Wrong type" );
	}

	const IdlistElement Coll::operator []( unsigned int ) const {
		throw collection_type_error( "Wrong type" );
	}

	Nary::Nary( Type type )
		: Coll( type )
	{
	}

	Nary::Nary( xmmsv_t* coll )
		: Coll( coll )
	{
	}

	Nary::~Nary()
	{
	}

	void Nary::addOperand( Coll& operand )
	{
		xmmsv_coll_add_operand( coll_, operand.getColl() );
	}

	void Nary::removeOperand( Coll& operand )
	{
		xmmsv_coll_remove_operand( coll_, operand.getColl() );
	}

	OperandIterator Nary::getOperandIterator()
	{
		return OperandIterator( *this );
	}

	const OperandIterator Nary::getOperandIterator() const
	{
		return OperandIterator( *this );
	}


	Unary::Unary( Type type )
		: Coll( type )
	{
	}

	Unary::Unary( Type type, Coll& operand )
		: Coll( type )
	{
		setOperand( operand );
	}

	Unary::Unary( xmmsv_t* coll )
		: Coll( coll )
	{
	}

	Unary::~Unary()
	{
	}

	void Unary::setOperand( Coll& operand )
	{
		removeOperand();
		xmmsv_coll_add_operand( coll_, operand.getColl() );
	}

	void Unary::removeOperand()
	{
		try {
			xmmsv_coll_remove_operand( coll_, (*getOperand()).getColl() );
		}
		/* don't throw an error if none */
		catch (...) {}
	}

	CollPtr Unary::getOperand() const
	{
		xmmsv_t *operands, *operand;

		// Find the operand
		operands = xmmsv_coll_operands_get( coll_ );
		if( !xmmsv_list_get( operands, 0, &operand ) ) {
			throw missing_operand_error( "No operand in this operator!" );
		}

		return CollResult::createColl( operand );
	}

	Filter::Filter( xmmsv_t* coll )
		: Unary( coll )
	{
	}

	Filter::Filter( Type operation )
		: Unary ( operation )
	{
	}

	Filter::Filter( Type operation, Coll& operand )
		: Unary ( operation, operand )
	{
	}

	Filter::Filter( Type operation, Coll& operand, const string& field )
		: Unary ( operation, operand )
	{
		setAttribute( "field", field );
	}

	Filter::Filter( Type operation,
	                Coll& operand,
	                const string& field,
	                const string& value )
		: Unary ( operation, operand )
	{
		setAttribute( "field", field );
		setAttribute( "value", value );
	}

	Filter::Filter( Type operation,
	                Coll& operand,
	                const string& field,
	                const string& value,
	                bool case_sensitive )
		: Unary ( operation, operand )
	{
		setAttribute( "field", field );
		setAttribute( "value", value );
		if( case_sensitive ) {
			setAttribute( "case-sensitive", "true" );
		}
	}

	Filter::~Filter() {}


	Reference::Reference()
		: Coll( REFERENCE )
	{
	}

	Reference::Reference( xmmsv_t* coll )
		: Coll( coll )
	{
	}

	Reference::Reference( const string& name,
	                      const Collection::Namespace& nsname )
		: Coll( REFERENCE )
	{
		setAttribute( "reference", name );
		setAttribute( "namespace", nsname );
	}

	Reference::~Reference()
	{
	}


	Universe::Universe( xmmsv_t* coll )
		: Coll( coll ) {}
	Universe::Universe()
		: Coll( UNIVERSE ) {}
	Universe::~Universe() {}

	Union::Union()
		: Nary( UNION ) {}
	Union::Union( xmmsv_t* coll )
		: Nary( coll ) {}
	Union::~Union() {}

	Intersection::Intersection()
		: Nary( INTERSECTION ) {}
	Intersection::Intersection( xmmsv_t* coll )
		: Nary( coll ) {}
	Intersection::~Intersection() {}

	Complement::Complement()
		: Unary( COMPLEMENT ) {}
	Complement::Complement( Coll& operand )
		: Unary( COMPLEMENT, operand ) {}
	Complement::Complement( xmmsv_t* coll )
		: Unary( coll ) {}
	Complement::~Complement() {}

	Has::Has()
		: Filter( HAS ) {}
	Has::Has( Coll& operand )
		: Filter( HAS, operand ) {}
	Has::Has( Coll& operand, const string& field )
		: Filter( HAS, operand, field ) {}
	Has::Has( xmmsv_t* coll )
		: Filter( coll ) {}
	Has::~Has() {}

	Smaller::Smaller()
		: Filter( SMALLER ) {}
	Smaller::Smaller( xmmsv_t* coll )
		: Filter( coll ) {}
	Smaller::Smaller( Coll& operand )
		: Filter( SMALLER, operand ) {}
	Smaller::Smaller( Coll& operand, const string& field )
		: Filter( SMALLER, operand, field ) {}
	Smaller::Smaller( Coll& operand,
	                  const string& field,
	                  const string& value )
		: Filter( SMALLER, operand, field, value ) {}
	Smaller::~Smaller() {}

	SmallerEqual::SmallerEqual()
		: Filter( SMALLEREQ ) {}
	SmallerEqual::SmallerEqual( xmmsv_t* coll )
		: Filter( coll ) {}
	SmallerEqual::SmallerEqual( Coll& operand )
		: Filter( SMALLEREQ, operand ) {}
	SmallerEqual::SmallerEqual( Coll& operand, const string& field )
		: Filter( SMALLEREQ, operand, field ) {}
	SmallerEqual::SmallerEqual( Coll& operand,
	                            const string& field,
	                            const string& value )
		: Filter( SMALLEREQ, operand, field, value ) {}
	SmallerEqual::~SmallerEqual() {}

	Greater::Greater()
		: Filter( GREATER ) {}
	Greater::Greater( xmmsv_t* coll )
		: Filter( coll ) {}
	Greater::Greater( Coll& operand )
		: Filter( GREATER, operand ) {}
	Greater::Greater( Coll& operand, const string& field )
		: Filter( GREATER, operand, field ) {}
	Greater::Greater( Coll& operand,
	                  const string& field,
	                  const string& value )
		: Filter( GREATER, operand, field, value ) {}
	Greater::~Greater() {}

	GreaterEqual::GreaterEqual()
		: Filter( GREATEREQ ) {}
	GreaterEqual::GreaterEqual( xmmsv_t* coll )
		: Filter( coll ) {}
	GreaterEqual::GreaterEqual( Coll& operand )
		: Filter( GREATEREQ, operand ) {}
	GreaterEqual::GreaterEqual( Coll& operand, const string& field )
		: Filter( GREATEREQ, operand, field ) {}
	GreaterEqual::GreaterEqual( Coll& operand,
	                            const string& field,
	                            const string& value )
		: Filter( GREATEREQ, operand, field, value ) {}
	GreaterEqual::~GreaterEqual() {}

	Equals::Equals()
		: Filter( EQUALS ) {}
	Equals::Equals( xmmsv_t* coll )
		: Filter( coll ) {}
	Equals::Equals( Coll& operand )
		: Filter( EQUALS, operand ) {}
	Equals::Equals( Coll& operand, const string& field )
		: Filter( EQUALS, operand, field ) {}
	Equals::Equals( Coll& operand,
	              const string& field,
	              const string& value,
	              bool case_sensitive )
		: Filter( EQUALS, operand, field, value, case_sensitive ) {}
	Equals::~Equals() {}

	NotEquals::NotEquals()
		: Filter( NOTEQUAL ) {}
	NotEquals::NotEquals( xmmsv_t* coll )
		: Filter( coll ) {}
	NotEquals::NotEquals( Coll& operand )
		: Filter( NOTEQUAL, operand ) {}
	NotEquals::NotEquals( Coll& operand, const string& field )
		: Filter( NOTEQUAL, operand, field ) {}
	NotEquals::NotEquals( Coll& operand,
	                const string& field,
	                const string& value,
	                bool case_sensitive )
		: Filter( NOTEQUAL,
		          operand, field, value, case_sensitive ) {}
	NotEquals::~NotEquals() {}

	Match::Match()
		: Filter( MATCH ) {}
	Match::Match( xmmsv_t* coll )
		: Filter( coll ) {}
	Match::Match( Coll& operand )
		: Filter( MATCH, operand ) {}
	Match::Match( Coll& operand, const string& field )
		: Filter( MATCH, operand, field ) {}
	Match::Match( Coll& operand,
	                    const string& field,
	                    const string& value,
	                    bool case_sensitive )
		: Filter( MATCH,
	              operand, field, value, case_sensitive ) {}
	Match::~Match() {}

	Token::Token()
			: Filter( TOKEN ) {}
	Token::Token( xmmsv_t* coll )
			: Filter( coll ) {}
	Token::Token( Coll& operand )
			: Filter( TOKEN, operand ) {}
	Token::Token( Coll& operand, const string& field )
			: Filter( TOKEN, operand, field ) {}
	Token::Token( Coll& operand,
		              const string& field,
		              const string& value,
		              bool case_sensitive )
			: Filter( TOKEN,
			          operand, field, value, case_sensitive ) {}
	Token::~Token() {}

	Order::Order()
		: Unary( ORDER )
	{
		setAttribute( "type", "random" );
	}
	Order::Order( Coll& operand )
		: Unary( ORDER, operand )
	{
		setAttribute( "type", "random" );
	}
	Order::Order( string& field, bool is_asc )
		: Unary( ORDER )
	{
		setAttribute( "type", "value" );
		setAttribute( "field", field );
		if( !is_asc ) {
			setAttribute( "order", "DESC" );
		}
	}
	Order::Order( Coll& operand, string& field, bool is_asc )
		: Unary( ORDER, operand )
	{
		setAttribute( "type", "value" );
		setAttribute( "field", field );
		if( !is_asc ) {
			setAttribute( "order", "DESC" );
		}
	}
	Order::Order( xmmsv_t* coll )
		: Unary( coll ) {}
	Order::~Order() {}

	Limit::Limit()
		: Unary( LIMIT ) {}
	Limit::Limit( Coll& operand )
		: Unary( LIMIT, operand ) {}
	Limit::Limit( Coll& operand, int start, int length)
		: Unary( LIMIT, operand )
	{
		std::stringstream _start;
		std::stringstream _length;

		_start << start;
		_length << length;

		setAttribute( "start", _start.str() );
		setAttribute( "length", _length.str() );
	}
	Limit::Limit( xmmsv_t* coll )
		: Unary( coll ) {}
	Limit::~Limit() {}

	Mediaset::Mediaset()
			: Unary( MEDIASET ) {}
	Mediaset::Mediaset( Coll& operand )
			: Unary( MEDIASET, operand ) {}
	Mediaset::Mediaset( xmmsv_t* coll )
			: Unary( coll ) {}
	Mediaset::~Mediaset() {}

	Idlist::Idlist( xmmsv_t* coll )
		: Coll( coll )
	{
	}
	Idlist::Idlist( const string& type )
		: Coll( IDLIST )
	{
		setAttribute( "type", type );
	}
	Idlist::Idlist()
		: Coll( IDLIST )
	{
		setAttribute( "type", "list" );
	}

	/* FIXME: copies
	Idlist::Idlist( const Idlist& src )
		: coll_( src.coll_ )
	{
		coll_.ref();
	}

	Idlist Idlist::operator=( const Idlist& src ) const
	{
		coll_.unref();
		coll_ = src.coll_;
		coll_.ref();
		return *this;
	}
	*/

	Idlist::~Idlist()
	{
	}

	Queue::Queue( xmmsv_t* coll )
		: Idlist( coll ) {}
	Queue::Queue( const string& type )
		: Idlist( type ) {}
	Queue::Queue( const string& type, unsigned int history )
		: Idlist( type )
	{
		setAttribute( "history", boost::lexical_cast<string>( history ) );
	}
	Queue::Queue()
		: Idlist( "queue" )
	{
	}
	Queue::Queue( unsigned int history )
		: Idlist( "queue" )
	{
		setAttribute( "history", boost::lexical_cast<string>( history ) );
	}
	Queue::~Queue()
	{
	}

	PartyShuffle::PartyShuffle( xmmsv_t* coll )
		: Queue( coll ) {}
	PartyShuffle::PartyShuffle()
		: Queue( "partyshuffle" )
	{
	}
	PartyShuffle::PartyShuffle( unsigned int history )
		: Queue( "partyshuffle", history )
	{
	}
	PartyShuffle::PartyShuffle( unsigned int history, unsigned int upcoming )
		: Queue( "partyshuffle", history )
	{
		setAttribute( "upcoming", boost::lexical_cast<string>( upcoming ) );
	}
	PartyShuffle::~PartyShuffle()
	{
	}

	void Idlist::append( int id )
	{
		if( !xmmsv_coll_idlist_append( coll_, id ) ) {
			std::stringstream err;
			err << "Failed to append " << id << " to idlist";
			throw collection_operation_error( err.str() );
		}
	}

	void Idlist::insert( unsigned int index, int id )
	{
		if( !xmmsv_coll_idlist_insert( coll_, index, id ) ) {
			std::stringstream err;
			err << "Failed to insert " << id << " in idlist at index " << index;
			throw collection_operation_error( err.str() );
		}
	}

	void Idlist::move( unsigned int index, unsigned int newindex )
	{
		if( !xmmsv_coll_idlist_move( coll_, index, newindex ) ) {
			std::stringstream err;
			err << "Failed to move idlist entry from index " << index
			    << " to " << newindex;
			throw collection_operation_error( err.str() );
		}
	}

	void Idlist::remove( unsigned int index )
	{
		if( !xmmsv_coll_idlist_remove( coll_, index ) ) {
			std::stringstream err;
			err << "Failed to remove idlist entry at index " << index;
			throw collection_operation_error( err.str() );
		}
	}

	void Idlist::clear()
	{
		if( !xmmsv_coll_idlist_clear( coll_ ) ) {
			throw collection_operation_error( "Failed to clear the idlist" );
		}
	}

	unsigned int Idlist::size() const
	{
		return xmmsv_coll_idlist_get_size( coll_ );
	}

	// get/set value at index
	IdlistElement Idlist::operator []( unsigned int index )
	{
		return IdlistElement( *this, index );
	}

	const IdlistElement Idlist::operator []( unsigned int index ) const
	{
		return IdlistElement( *this, index );
	}

	IdlistElement::IdlistElement( Coll& coll, unsigned int index )
		: AbstractElement< unsigned int, int >( coll, index )
	{
	}

	IdlistElement::IdlistElement( const Coll& coll, unsigned int index )
		: AbstractElement< unsigned int, int >( const_cast< Coll& >( coll ),
		                                        index )
	{
	}

	IdlistElement::~IdlistElement()
	{
	}

	IdlistElement::operator int() const
	{
		return coll_.getIndex( index_ );
	}

	int
	IdlistElement::operator=( int value )
	{
		coll_.setIndex( index_, value );
		return value;
	}

	void PartyShuffle::setOperand( Coll& operand )
	{
		removeOperand();
		xmmsv_coll_add_operand( coll_, operand.getColl() );
	}

	void PartyShuffle::removeOperand()
	{
		try {
			xmmsv_coll_remove_operand( coll_, (*getOperand()).getColl() );
		}
		// don't throw an error if none
		catch (...) {}
	}

	CollPtr PartyShuffle::getOperand() const
	{
		xmmsv_t *operands, *operand;

		// Find the operand
		operands = xmmsv_coll_operands_get( coll_ );
		if( !xmmsv_list_get( operands, 0, &operand ) ) {
			throw missing_operand_error( "No operand in this operator!" );
		}

		return CollResult::createColl( operand );
	}


	OperandIterator::OperandIterator( Coll& coll )
		: coll_( coll )
	{
		coll_.ref();
		initIterator();
	}

	OperandIterator::OperandIterator( const Coll& coll )
		: coll_( const_cast< Coll& >( coll ) )
	{
		coll_.ref();
		initIterator();
	}

	OperandIterator::OperandIterator( const OperandIterator& src )
		: coll_( src.coll_ )
	{
		coll_.ref();

		// We init a new iterator so the copy is independent
		initIterator();
	}

	OperandIterator OperandIterator::operator=( const OperandIterator& src )
	{
		coll_.unref();
		coll_ = src.coll_;
		oper_it_ = src.oper_it_;
		coll_.ref();
		return *this;
	}

	OperandIterator::~OperandIterator()
	{
		coll_.unref();

		// The list iterators are automatically freed.
	}

	void OperandIterator::initIterator()
	{
		xmmsv_t *operands( xmmsv_coll_operands_get( coll_.coll_ ) );
		xmmsv_get_list_iter( operands, &oper_it_ );
	}

	void OperandIterator::first()
	{
		xmmsv_list_iter_first( oper_it_ );
	}

	bool OperandIterator::valid() const
	{
		return xmmsv_list_iter_valid( oper_it_ );
	}

	void OperandIterator::next()
	{
		xmmsv_list_iter_next( oper_it_ );
	}

	CollPtr OperandIterator::operator *() const
	{
		xmmsv_t *operand;
		if( !xmmsv_list_iter_entry( oper_it_, &operand ) ) {
			throw out_of_range( "Access out of the operand list!" );
		}

		return CollResult::createColl( operand );
	}



	AttributeElement::AttributeElement( Coll& coll, string index )
		: AbstractElement< string, string >( coll, index )
	{
	}

	AttributeElement::AttributeElement( const Coll& coll, string index )
		: AbstractElement< string, string >( const_cast< Coll& >( coll ), index )
	{
	}

	AttributeElement::~AttributeElement()
	{
	}

	AttributeElement::operator string() const
	{
		return coll_.getAttribute( index_ );
	}

	string
	AttributeElement::operator=( string value )
	{
		coll_.setAttribute( index_, value );
		return value;
	}

	}
}
