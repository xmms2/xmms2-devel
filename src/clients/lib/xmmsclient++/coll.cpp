
#include <xmmsclient/xmmsclient.h>
#include <xmmsclient/xmmsclient++/typedefs.h>
#include <xmmsclient/xmmsclient++/exceptions.h>
#include <xmmsclient/xmmsclient++/coll.h>

#include <string>
#include <sstream>
#include <stdexcept>

namespace Xmms
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

	void Coll::addOperand( Coll& operand )
	{
		xmmsc_coll_add_operand( coll_, operand.coll_ );
	}

	void Coll::removeOperand( Coll& operand )
	{
		xmmsc_coll_remove_operand( coll_, operand.coll_ );
	}

	Coll::OperandIterator Coll::getOperandIterator()
	{
		return OperandIterator( *this );
	}

	Coll::AttributeElement Coll::operator []( const std::string& attrname )
	{
		return AttributeElement( *this, attrname );
	}

	void Coll::setAttribute( const std::string &attrname, const std::string &value )
	{
		xmmsc_coll_attribute_set( coll_, attrname.c_str(), value.c_str() );
	}

	std::string Coll::getAttribute( const std::string &attrname )
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

	Coll::Idlist Coll::getIdlist()
	{
		return Coll::Idlist( *this );
	}

	void Coll::idlistSetIndex( unsigned int index, unsigned int value )
	{
		if( !xmmsc_coll_idlist_set_index( coll_, index, value ) ) {
			std::stringstream err;
			err << "Index out of idlist: "  << index;
			throw out_of_range( err.str() );
		}
	}

	unsigned int Coll::idlistGetIndex( unsigned int index )
	{
		unsigned int value;
		if( !xmmsc_coll_idlist_get_index( coll_, index, &value ) ) {
			std::stringstream err;
			err << "Index out of idlist: "  << index;
			throw out_of_range( err.str() );
		}

		return value;
	}



	Coll::OperandIterator::OperandIterator( Coll& coll )
		: coll_( coll )
	{
		coll_.ref();
	}

	Coll::OperandIterator::OperandIterator( const Coll::OperandIterator& src )
		: coll_( src.coll_ )
	{
		coll_.ref();
	}

	Coll::OperandIterator Coll::OperandIterator::operator=( const Coll::OperandIterator& src ) const
	{
		coll_.unref();
		coll_ = src.coll_;
		coll_.ref();
		return *this;
	}

	Coll::OperandIterator::~OperandIterator()
	{
		coll_.unref();
	}

	void Coll::OperandIterator::first()
	{
		if( !xmmsc_coll_operand_list_first( coll_.coll_ ) ) {
			throw out_of_range( "Access out of the operand list!" );
		}
	}

	bool Coll::OperandIterator::valid()
	{
		return xmmsc_coll_operand_list_valid( coll_.coll_ );
	}

	void Coll::OperandIterator::next()
	{
		if( !xmmsc_coll_operand_list_next( coll_.coll_ ) ) {
			throw out_of_range( "Access out of the operand list!" );
		}
	}

	void Coll::OperandIterator::save()
	{
		if( !xmmsc_coll_operand_list_save( coll_.coll_ ) ) {
			throw out_of_range( "Access out of the operand list!" );
		}
	}

	void Coll::OperandIterator::restore()
	{
		if( !xmmsc_coll_operand_list_restore( coll_.coll_ ) ) {
			throw out_of_range( "Access out of the operand list!" );
		}
	}

	Coll Coll::OperandIterator::operator *()
	{
		xmmsc_coll_t *op;
		if( !xmmsc_coll_operand_list_entry( coll_.coll_, &op ) ) {
			throw out_of_range( "Access out of the operand list!" );
		}

		return Coll( op );
	}



	Coll::Idlist::Idlist( Coll& coll )
		: coll_( coll )
	{
		coll_.ref();
	}

	Coll::Idlist::Idlist( const Idlist& src )
		: coll_( src.coll_ )
	{
		coll_.ref();
	}

	Coll::Idlist Coll::Idlist::operator=( const Idlist& src ) const
	{
		coll_.unref();
		coll_ = src.coll_;
		coll_.ref();
		return *this;
	}

	Coll::Idlist::~Idlist()
	{
		coll_.unref();
	}

	void Coll::Idlist::append( unsigned int id )
	{
		if( !xmmsc_coll_idlist_append( coll_.coll_, id ) ) {
			std::stringstream err;
			err << "Failed to append " << id << " to idlist";
			throw std::runtime_error( err.str() );
		}
	}

	void Coll::Idlist::insert( unsigned int id, unsigned int index )
	{
		if( !xmmsc_coll_idlist_insert( coll_.coll_, id, index ) ) {
			std::stringstream err;
			err << "Failed to insert " << id << " in idlist at index " << index;
			throw std::runtime_error( err.str() );
		}
	}

	void Coll::Idlist::move( unsigned int index, unsigned int newindex )
	{
		if( !xmmsc_coll_idlist_move( coll_.coll_, index, newindex ) ) {
			std::stringstream err;
			err << "Failed to move idlist entry from index " << index
			    << " to " << newindex;
			throw std::runtime_error( err.str() );
		}
	}

	void Coll::Idlist::remove( unsigned int index )
	{
		if( !xmmsc_coll_idlist_remove( coll_.coll_, index ) ) {
			std::stringstream err;
			err << "Failed to remove idlist entry at index " << index;
			throw std::runtime_error( err.str() );
		}
	}

	void Coll::Idlist::clear()
	{
		if( !xmmsc_coll_idlist_clear( coll_.coll_ ) ) {
			throw std::runtime_error( "Failed to clear the idlist" );
		}
	}

	unsigned int Coll::Idlist::size() const
	{
		return xmmsc_coll_idlist_get_size( coll_.coll_ );
	}

	// get/set value at index
	Coll::Idlist::Element Coll::Idlist::operator []( unsigned int index )
	{
		return Element( coll_, index );
	}



	Coll::AttributeElement::AttributeElement( Coll& coll, std::string index )
		: AbstractElement< std::string, std::string >( coll, index )
	{
	}

	Coll::AttributeElement::~AttributeElement()
	{
	}

	Coll::AttributeElement::operator std::string() const
	{
		return coll_.getAttribute( index_ );
	}

	std::string
	Coll::AttributeElement::operator=( std::string value )
	{
		coll_.setAttribute( index_, value );
		return value;
	}


	Coll::Idlist::Element::Element( Coll& coll, unsigned int index )
		: AbstractElement< unsigned int, unsigned int >( coll, index )
	{
	}

	Coll::Idlist::Element::~Element()
	{
	}

	Coll::Idlist::Element::operator unsigned int() const
	{
		return coll_.idlistGetIndex( index_ );
	}

	unsigned int
	Coll::Idlist::Element::operator=( unsigned int value )
	{
		coll_.idlistSetIndex( index_, value );
		return value;
	}

}
