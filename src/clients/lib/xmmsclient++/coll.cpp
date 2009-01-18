/*  XMMS2 - X Music Multiplexer System
 *  Copyright (C) 2003-2008 XMMS2 Team
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
		coll_ = xmmsv_coll_new( type );
		if( !coll_ ) {
			throw std::runtime_error( "Failed to create a Coll object" );
		}
	}
	
	Coll::Coll( xmmsv_coll_t *coll )
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
		xmmsv_coll_ref( coll_ );
	}

	void Coll::unref()
	{
		xmmsv_coll_unref( coll_ );
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
		xmmsv_coll_attribute_set( coll_, attrname.c_str(), value.c_str() );
	}

	string Coll::getAttribute( const string &attrname ) const
	{
		char *val;
		if( !xmmsv_coll_attribute_get( coll_, attrname.c_str(), &val ) ) {
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

	void Coll::setIndex( unsigned int index, unsigned int value )
	{
		if( !xmmsv_coll_idlist_set_index( coll_, index, value ) ) {
			std::stringstream err;
			err << "Index out of idlist: "  << index;
			throw out_of_range( err.str() );
		}
	}

	unsigned int Coll::getIndex( unsigned int index ) const
	{
		unsigned int value;
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

	void Coll::append( unsigned int ) {
		throw collection_type_error( "Wrong type" );
	}

	void Coll::insert( unsigned int, unsigned int ) {
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

	Nary::Nary( xmmsv_coll_t* coll )
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

	Unary::Unary( xmmsv_coll_t* coll )
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
		xmmsv_coll_t *op;

		// Find the operand
		xmmsv_coll_operand_list_save( coll_ );
		xmmsv_coll_operand_list_first( coll_ );
		if( !xmmsv_coll_operand_list_entry( coll_, &op ) ) {
			op = NULL;
		}
		xmmsv_coll_operand_list_restore( coll_ );

		if( !op ) {
			throw missing_operand_error( "No operand in this operator!" );
		}

		return CollResult::createColl( op );
	}

	Filter::Filter( xmmsv_coll_t* coll )
		: Unary( coll )
	{
	}

	Filter::Filter( Type type )
		: Unary ( type )
	{
	}

	Filter::Filter( Type type, Coll& operand )
		: Unary ( type, operand )
	{
	}

	Filter::Filter( Type type, Coll& operand, const string& field )
		: Unary ( type, operand )
	{
		setAttribute( "field", field );
	}

	Filter::Filter( Type type,
	                Coll& operand,
	                const string& field,
	                const string& value )
		: Unary ( type, operand )
	{
		setAttribute( "field", field );
		setAttribute( "value", value );
	}

	Filter::Filter( Type type,
	                Coll& operand,
	                const string& field,
	                const string& value,
	                bool case_sensitive )
		: Unary ( type, operand )
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

	Reference::Reference( xmmsv_coll_t* coll )
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


	Universe::Universe()
		: Reference( "All Media", Collection::COLLECTIONS ) {}
	Universe::~Universe() {}

	Union::Union()
		: Nary( UNION ) {}
	Union::Union( xmmsv_coll_t* coll )
		: Nary( coll ) {}
	Union::~Union() {}

	Intersection::Intersection()
		: Nary( INTERSECTION ) {}
	Intersection::Intersection( xmmsv_coll_t* coll )
		: Nary( coll ) {}
	Intersection::~Intersection() {}

	Complement::Complement()
		: Unary( COMPLEMENT ) {}
	Complement::Complement( Coll& operand )
		: Unary( COMPLEMENT, operand ) {}
	Complement::Complement( xmmsv_coll_t* coll )
		: Unary( coll ) {}
	Complement::~Complement() {}

	Has::Has()
		: Filter( HAS ) {}
	Has::Has( Coll& operand )
		: Filter( HAS, operand ) {}
	Has::Has( Coll& operand, const string& field )
		: Filter( HAS, operand, field ) {}
	Has::Has( xmmsv_coll_t* coll )
		: Filter( coll ) {}
	Has::~Has() {}

	Smaller::Smaller()
		: Filter( SMALLER ) {}
	Smaller::Smaller( xmmsv_coll_t* coll )
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

	Greater::Greater()
		: Filter( GREATER ) {}
	Greater::Greater( xmmsv_coll_t* coll )
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

	Equals::Equals()
		: Filter( EQUALS ) {}
	Equals::Equals( xmmsv_coll_t* coll )
		: Filter( coll ) {}
	Equals::Equals( Coll& operand )
		: Filter( EQUALS, operand ) {}
	Equals::Equals( Coll& operand, const string& field )
		: Filter( EQUALS, operand, field ) {}
	Equals::Equals( Coll& operand,
	              const string& field,
	              const string& value,
	              bool case_sensitive )
		: Filter( EQUALS,
	              operand, field, value, case_sensitive ) {}
	Equals::~Equals() {}

	Match::Match()
		: Filter( MATCH ) {}
	Match::Match( xmmsv_coll_t* coll )
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


	Idlist::Idlist( xmmsv_coll_t* coll )
		: Coll( coll )
	{
	}
	Idlist::Idlist( Type type )
		: Coll( type )
	{
	}
	Idlist::Idlist()
		: Coll( IDLIST )
	{
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

	Queue::Queue( xmmsv_coll_t* coll )
		: Idlist( coll ) {}
	Queue::Queue( Type type )
		: Idlist( type ) {}
	Queue::Queue( Type type, unsigned int history )
		: Idlist( type )
	{
		setAttribute( "history", boost::lexical_cast<string>( history ) );
	}
	Queue::Queue()
		: Idlist( QUEUE )
	{
	}
	Queue::Queue( unsigned int history )
		: Idlist( QUEUE )
	{
		setAttribute( "history", boost::lexical_cast<string>( history ) );
	}
	Queue::~Queue()
	{
	}

	PartyShuffle::PartyShuffle( xmmsv_coll_t* coll )
		: Queue( coll ) {}
	PartyShuffle::PartyShuffle()
		: Queue( PARTYSHUFFLE )
	{
	}
	PartyShuffle::PartyShuffle( unsigned int history )
		: Queue( PARTYSHUFFLE, history )
	{
	}
	PartyShuffle::PartyShuffle( unsigned int history, unsigned int upcoming )
		: Queue( PARTYSHUFFLE, history )
	{
		setAttribute( "upcoming", boost::lexical_cast<string>( upcoming ) );
	}
	PartyShuffle::~PartyShuffle()
	{
	}


	void Idlist::append( unsigned int id )
	{
		if( !xmmsv_coll_idlist_append( coll_, id ) ) {
			std::stringstream err;
			err << "Failed to append " << id << " to idlist";
			throw collection_operation_error( err.str() );
		}
	}

	void Idlist::insert( unsigned int index, unsigned int id )
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
		: AbstractElement< unsigned int, unsigned int >( coll, index )
	{
	}

	IdlistElement::IdlistElement( const Coll& coll, unsigned int index )
		: AbstractElement< unsigned int, unsigned int >( const_cast< Coll& >( coll ),
	                                                     index )
	{
	}

	IdlistElement::~IdlistElement()
	{
	}

	IdlistElement::operator unsigned int() const
	{
		return coll_.getIndex( index_ );
	}

	unsigned int
	IdlistElement::operator=( unsigned int value )
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
		/* don't throw an error if none */
		catch (...) {}
	}

	CollPtr PartyShuffle::getOperand() const
	{
		xmmsv_coll_t *op;

		// Find the operand
		xmmsv_coll_operand_list_save( coll_ );
		xmmsv_coll_operand_list_first( coll_ );
		if( !xmmsv_coll_operand_list_entry( coll_, &op ) ) {
			op = NULL;
		}
		xmmsv_coll_operand_list_restore( coll_ );

		if( !op ) {
			throw missing_operand_error( "No operand in this operator!" );
		}

		return CollResult::createColl( op );
	}



	OperandIterator::OperandIterator( Coll& coll )
		: coll_( coll )
	{
		coll_.ref();
	}

	OperandIterator::OperandIterator( const Coll& coll )
		: coll_( const_cast< Coll& >( coll ) )
	{
		coll_.ref();
	}

	OperandIterator::OperandIterator( const OperandIterator& src )
		: coll_( src.coll_ )
	{
		coll_.ref();
	}

	OperandIterator OperandIterator::operator=( const OperandIterator& src ) const
	{
		coll_.unref();
		coll_ = src.coll_;
		coll_.ref();
		return *this;
	}

	OperandIterator::~OperandIterator()
	{
		coll_.unref();
	}

	void OperandIterator::first()
	{
		if( !xmmsv_coll_operand_list_first( coll_.coll_ ) ) {
			throw out_of_range( "Access out of the operand list!" );
		}
	}

	bool OperandIterator::valid() const
	{
		return xmmsv_coll_operand_list_valid( coll_.coll_ );
	}

	void OperandIterator::next()
	{
		if( !xmmsv_coll_operand_list_next( coll_.coll_ ) ) {
			throw out_of_range( "Access out of the operand list!" );
		}
	}

	void OperandIterator::save()
	{
		if( !xmmsv_coll_operand_list_save( coll_.coll_ ) ) {
			throw out_of_range( "Access out of the operand list!" );
		}
	}

	void OperandIterator::restore()
	{
		if( !xmmsv_coll_operand_list_restore( coll_.coll_ ) ) {
			throw out_of_range( "Access out of the operand list!" );
		}
	}

	CollPtr OperandIterator::operator *() const
	{
		xmmsv_coll_t *op;
		if( !xmmsv_coll_operand_list_entry( coll_.coll_, &op ) ) {
			throw out_of_range( "Access out of the operand list!" );
		}

		return CollResult::createColl( op );
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
