
#include <xmmsclient/xmmsclient.h>
#include <xmmsclient/xmmsclient++/typedefs.h>
#include <xmmsclient/xmmsclient++/exceptions.h>
#include <xmmsclient/xmmsclient++/coll.h>

#include <string>
#include <sstream>
#include <stdexcept>
#include <boost/lexical_cast.hpp>

namespace Xmms
{

	namespace Coll
	{

	Coll::Coll( Type type )
	{
		coll_ = xmmsc_coll_new( type );
		if( !coll_ ) {
			throw std::runtime_error( "Failed to create a Coll object" );
		}
	}
	
	Coll::Coll( xmmsc_coll_t *coll )
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
		xmmsc_coll_ref( coll_ );
	}

	void Coll::unref()
	{
		xmmsc_coll_unref( coll_ );
	}

	Type Coll::getType() const {
		return xmmsc_coll_get_type( coll_ );
	}

	AttributeElement Coll::operator []( const std::string& attrname )
	{
		return AttributeElement( *this, attrname );
	}

	void Coll::setAttribute( const std::string &attrname, const std::string &value )
	{
		xmmsc_coll_attribute_set( coll_, attrname.c_str(), value.c_str() );
	}

	std::string Coll::getAttribute( const std::string &attrname ) const
	{
		char *val;
		if( !xmmsc_coll_attribute_get( coll_, attrname.c_str(), &val ) ) {
			throw no_such_key_error( "No such attribute: " + attrname );
		}

		return std::string( val );
	}

	void Coll::removeAttribute( const std::string &attrname )
	{
		if( !xmmsc_coll_attribute_remove( coll_, attrname.c_str() ) ) {
			throw no_such_key_error( "No such attribute: " + attrname );
		}
	}

	void Coll::setIndex( unsigned int index, unsigned int value )
	{
		if( !xmmsc_coll_idlist_set_index( coll_, index, value ) ) {
			std::stringstream err;
			err << "Index out of idlist: "  << index;
			throw out_of_range( err.str() );
		}
	}

	unsigned int Coll::getIndex( unsigned int index ) const
	{
		unsigned int value;
		if( !xmmsc_coll_idlist_get_index( coll_, index, &value ) ) {
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

	Coll Coll::getOperand() const {
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

	IdlistElement Coll::operator[]( unsigned int ) {
		throw collection_type_error( "Wrong type" );
	}

	Nary::Nary( Type type )
		: Coll( type )
	{
	}

	Nary::Nary( xmmsc_coll_t* coll )
		: Coll( coll )
	{
	}

	Nary::~Nary()
	{
	}

	void Nary::addOperand( Coll& operand )
	{
		xmmsc_coll_add_operand( coll_, operand.getColl() );
	}

	void Nary::removeOperand( Coll& operand )
	{
		xmmsc_coll_remove_operand( coll_, operand.getColl() );
	}

	OperandIterator Nary::getOperandIterator()
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

	Unary::Unary( xmmsc_coll_t* coll )
		: Coll( coll )
	{
	}

	Unary::~Unary()
	{
	}

	void Unary::setOperand( Coll& operand )
	{
		removeOperand();
		xmmsc_coll_add_operand( coll_, operand.getColl() );
	}

	void Unary::removeOperand()
	{
		try {
			xmmsc_coll_remove_operand( coll_, getOperand().getColl() );
		}
		/* don't throw an error if none */
		catch (...) {}
	}

	Coll Unary::getOperand() const
	{
		xmmsc_coll_t *op;

		// Find the operand
		xmmsc_coll_operand_list_save( coll_ );
		xmmsc_coll_operand_list_first( coll_ );
		if( !xmmsc_coll_operand_list_entry( coll_, &op ) ) {
			op = NULL;
		}
		xmmsc_coll_operand_list_restore( coll_ );

		if( !op ) {
			throw missing_operand_error( "No operand in this operator!" );
		}

		return Coll( op );
	}

	Filter::Filter( xmmsc_coll_t* coll )
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

	Filter::Filter( Type type, Coll& operand, const std::string& field )
		: Unary ( type, operand )
	{
		setAttribute( "field", field );
	}

	Filter::Filter( Type type,
	                Coll& operand,
	                const std::string& field,
	                const std::string& value )
		: Unary ( type, operand )
	{
		setAttribute( "field", field );
		setAttribute( "value", value );
	}

	Filter::Filter( Type type,
	                Coll& operand,
	                const std::string& field,
	                const std::string& value,
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
		: Coll( XMMS_COLLECTION_TYPE_REFERENCE )
	{
	}

	Reference::Reference( xmmsc_coll_t* coll )
		: Coll( coll )
	{
	}

	Reference::Reference( const std::string& name,
	                      const Collection::Namespace& nsname )
		: Coll( XMMS_COLLECTION_TYPE_REFERENCE )
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
		: Nary( XMMS_COLLECTION_TYPE_UNION ) {}
	Union::Union( xmmsc_coll_t* coll )
		: Nary( coll ) {}
	Union::~Union() {}

	Intersection::Intersection()
		: Nary( XMMS_COLLECTION_TYPE_INTERSECTION ) {}
	Intersection::Intersection( xmmsc_coll_t* coll )
		: Nary( coll ) {}
	Intersection::~Intersection() {}

	Complement::Complement()
		: Unary( XMMS_COLLECTION_TYPE_COMPLEMENT ) {}
	Complement::Complement( Coll& operand )
		: Unary( XMMS_COLLECTION_TYPE_COMPLEMENT, operand ) {}
	Complement::Complement( xmmsc_coll_t* coll )
		: Unary( coll ) {}
	Complement::~Complement() {}

	Has::Has()
		: Filter( XMMS_COLLECTION_TYPE_HAS ) {}
	Has::Has( Coll& operand )
		: Filter( XMMS_COLLECTION_TYPE_HAS, operand ) {}
	Has::Has( Coll& operand, const std::string& field )
		: Filter( XMMS_COLLECTION_TYPE_HAS, operand, field ) {}
	Has::Has( xmmsc_coll_t* coll )
		: Filter( coll ) {}
	Has::~Has() {}

	Smaller::Smaller()
		: Filter( XMMS_COLLECTION_TYPE_SMALLER ) {}
	Smaller::Smaller( xmmsc_coll_t* coll )
		: Filter( coll ) {}
	Smaller::Smaller( Coll& operand )
		: Filter( XMMS_COLLECTION_TYPE_SMALLER, operand ) {}
	Smaller::Smaller( Coll& operand, const std::string& field )
		: Filter( XMMS_COLLECTION_TYPE_SMALLER, operand, field ) {}
	Smaller::Smaller( Coll& operand,
	                  const std::string& field,
	                  const std::string& value )
		: Filter( XMMS_COLLECTION_TYPE_SMALLER, operand, field, value ) {}
	Smaller::~Smaller() {}

	Greater::Greater()
		: Filter( XMMS_COLLECTION_TYPE_GREATER ) {}
	Greater::Greater( xmmsc_coll_t* coll )
		: Filter( coll ) {}
	Greater::Greater( Coll& operand )
		: Filter( XMMS_COLLECTION_TYPE_GREATER, operand ) {}
	Greater::Greater( Coll& operand, const std::string& field )
		: Filter( XMMS_COLLECTION_TYPE_GREATER, operand, field ) {}
	Greater::Greater( Coll& operand,
	                  const std::string& field,
	                  const std::string& value )
		: Filter( XMMS_COLLECTION_TYPE_GREATER, operand, field, value ) {}
	Greater::~Greater() {}

	Match::Match()
		: Filter( XMMS_COLLECTION_TYPE_MATCH ) {}
	Match::Match( xmmsc_coll_t* coll )
		: Filter( coll ) {}
	Match::Match( Coll& operand )
		: Filter( XMMS_COLLECTION_TYPE_MATCH, operand ) {}
	Match::Match( Coll& operand, const std::string& field )
		: Filter( XMMS_COLLECTION_TYPE_MATCH, operand, field ) {}
	Match::Match( Coll& operand,
	              const std::string& field,
	              const std::string& value,
	              bool case_sensitive )
		: Filter( XMMS_COLLECTION_TYPE_MATCH,
	              operand, field, value, case_sensitive ) {}
	Match::~Match() {}

	Contains::Contains()
		: Filter( XMMS_COLLECTION_TYPE_CONTAINS ) {}
	Contains::Contains( xmmsc_coll_t* coll )
		: Filter( coll ) {}
	Contains::Contains( Coll& operand )
		: Filter( XMMS_COLLECTION_TYPE_CONTAINS, operand ) {}
	Contains::Contains( Coll& operand, const std::string& field )
		: Filter( XMMS_COLLECTION_TYPE_CONTAINS, operand, field ) {}
	Contains::Contains( Coll& operand,
	                    const std::string& field,
	                    const std::string& value,
	                    bool case_sensitive )
		: Filter( XMMS_COLLECTION_TYPE_CONTAINS,
	              operand, field, value, case_sensitive ) {}
	Contains::~Contains() {}


	Idlist::Idlist( xmmsc_coll_t* coll )
		: Coll( coll )
	{
	}
	Idlist::Idlist( Type type )
		: Coll( type )
	{
	}
	Idlist::Idlist()
		: Coll( XMMS_COLLECTION_TYPE_IDLIST )
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

	Queue::Queue( xmmsc_coll_t* coll )
		: Idlist( coll ) {}
	Queue::Queue( Type type )
		: Idlist( type ) {}
	Queue::Queue( Type type, unsigned int history )
		: Idlist( type )
	{
		setAttribute( "history", boost::lexical_cast<std::string>( history ) );
	}
	Queue::Queue()
		: Idlist( XMMS_COLLECTION_TYPE_QUEUE )
	{
	}
	Queue::Queue( unsigned int history )
		: Idlist( XMMS_COLLECTION_TYPE_QUEUE )
	{
		setAttribute( "history", boost::lexical_cast<std::string>( history ) );
	}
	Queue::~Queue()
	{
	}

	PartyShuffle::PartyShuffle( xmmsc_coll_t* coll )
		: Queue( coll ) {}
	PartyShuffle::PartyShuffle()
		: Queue( XMMS_COLLECTION_TYPE_QUEUE )
	{
	}
	PartyShuffle::PartyShuffle( unsigned int history )
		: Queue( XMMS_COLLECTION_TYPE_QUEUE )
	{
		setAttribute( "history", boost::lexical_cast<std::string>( history ) );
	}
	PartyShuffle::PartyShuffle( unsigned int history, unsigned int upcoming )
		: Queue( XMMS_COLLECTION_TYPE_QUEUE )
	{
		setAttribute( "history", boost::lexical_cast<std::string>( history ) );
		setAttribute( "upcoming", boost::lexical_cast<std::string>( upcoming ) );
	}
	PartyShuffle::~PartyShuffle()
	{
	}


	void Idlist::append( unsigned int id )
	{
		if( !xmmsc_coll_idlist_append( coll_, id ) ) {
			std::stringstream err;
			err << "Failed to append " << id << " to idlist";
			throw collection_operation_error( err.str() );
		}
	}

	void Idlist::insert( unsigned int id, unsigned int index )
	{
		if( !xmmsc_coll_idlist_insert( coll_, id, index ) ) {
			std::stringstream err;
			err << "Failed to insert " << id << " in idlist at index " << index;
			throw collection_operation_error( err.str() );
		}
	}

	void Idlist::move( unsigned int index, unsigned int newindex )
	{
		if( !xmmsc_coll_idlist_move( coll_, index, newindex ) ) {
			std::stringstream err;
			err << "Failed to move idlist entry from index " << index
			    << " to " << newindex;
			throw collection_operation_error( err.str() );
		}
	}

	void Idlist::remove( unsigned int index )
	{
		if( !xmmsc_coll_idlist_remove( coll_, index ) ) {
			std::stringstream err;
			err << "Failed to remove idlist entry at index " << index;
			throw collection_operation_error( err.str() );
		}
	}

	void Idlist::clear()
	{
		if( !xmmsc_coll_idlist_clear( coll_ ) ) {
			throw collection_operation_error( "Failed to clear the idlist" );
		}
	}

	unsigned int Idlist::size() const
	{
		return xmmsc_coll_idlist_get_size( coll_ );
	}

	// get/set value at index
	IdlistElement Idlist::operator []( unsigned int index )
	{
		return IdlistElement( *this, index );
	}

	IdlistElement::IdlistElement( Coll& coll, unsigned int index )
		: AbstractElement< unsigned int, unsigned int >( coll, index )
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



	OperandIterator::OperandIterator( Coll& coll )
		: coll_( coll )
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
		if( !xmmsc_coll_operand_list_first( coll_.coll_ ) ) {
			throw out_of_range( "Access out of the operand list!" );
		}
	}

	bool OperandIterator::valid() const
	{
		return xmmsc_coll_operand_list_valid( coll_.coll_ );
	}

	void OperandIterator::next()
	{
		if( !xmmsc_coll_operand_list_next( coll_.coll_ ) ) {
			throw out_of_range( "Access out of the operand list!" );
		}
	}

	void OperandIterator::save()
	{
		if( !xmmsc_coll_operand_list_save( coll_.coll_ ) ) {
			throw out_of_range( "Access out of the operand list!" );
		}
	}

	void OperandIterator::restore()
	{
		if( !xmmsc_coll_operand_list_restore( coll_.coll_ ) ) {
			throw out_of_range( "Access out of the operand list!" );
		}
	}

	Coll OperandIterator::operator *() const
	{
		xmmsc_coll_t *op;
		if( !xmmsc_coll_operand_list_entry( coll_.coll_, &op ) ) {
			throw out_of_range( "Access out of the operand list!" );
		}

		return Coll( op );
	}



	AttributeElement::AttributeElement( Coll& coll, std::string index )
		: AbstractElement< std::string, std::string >( coll, index )
	{
	}

	AttributeElement::~AttributeElement()
	{
	}

	AttributeElement::operator std::string() const
	{
		return coll_.getAttribute( index_ );
	}

	std::string
	AttributeElement::operator=( std::string value )
	{
		coll_.setAttribute( index_, value );
		return value;
	}

	}
}
