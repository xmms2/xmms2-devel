
#include <xmmsclient/xmmsclient.h>
#include <xmmsclient/xmmsclient++/typedefs.h>
#include <xmmsclient/xmmsclient++/exceptions.h>
#include <xmmsclient/xmmsclient++/coll.h>

#include <string>
#include <sstream>

namespace Xmms
{
	
	Coll::Coll( Type type )
	{
		coll_ = xmmsc_coll_new (type);
		/* FIXME: error, coll NULL ? */
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
		if (!xmmsc_coll_attribute_get( coll_, attrname.c_str(), &val )) {
			throw no_such_key_error( "No such attribute: " + attrname );
		}

		return std::string( val );
	}

	Coll::Idlist Coll::getIdlist()
	{
		return Coll::Idlist( *this );
	}

	void Coll::idlistSetIndex( unsigned int index, unsigned int value )
	{
		/* FIXME: suppress error message on stderr ! */
		if (!xmmsc_coll_idlist_set_index( coll_, index, value )) {
			std::stringstream err;
			err << "Index out of idlist: "  << index;
			throw out_of_range( err.str() );
		}
	}

	unsigned int Coll::idlistGetIndex( unsigned int index )
	{
		/* FIXME: suppress error message on stderr ! */
		unsigned int value;
		if (!xmmsc_coll_idlist_get_index( coll_, index, &value )) {
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
		/* FIXME: unref previous? */
		coll_ = src.coll_;
		coll_.ref();
		return *this;
	}

	Coll::OperandIterator::~OperandIterator()
	{
		coll_.unref();
	}

	bool Coll::OperandIterator::first()
	{
		return xmmsc_coll_operand_list_first( coll_.coll_ );
	}

	bool Coll::OperandIterator::valid()
	{
		xmmsc_coll_t *tmp;
		return xmmsc_coll_operand_list_entry( coll_.coll_, &tmp );
	}

	bool Coll::OperandIterator::next()
	{
		return xmmsc_coll_operand_list_next( coll_.coll_ );
	}

	bool Coll::OperandIterator::save()
	{
		return xmmsc_coll_operand_list_save( coll_.coll_ );
	}

	bool Coll::OperandIterator::restore()
	{
		return xmmsc_coll_operand_list_restore( coll_.coll_ );
	}

	Coll Coll::OperandIterator::operator *()
	{
		xmmsc_coll_t *op;
		if (!xmmsc_coll_operand_list_entry( coll_.coll_, &op )) {
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
		/* FIXME: unref previous? */
		coll_ = src.coll_;
		coll_.ref();
		return *this;
	}

	Coll::Idlist::~Idlist()
	{
		coll_.unref();
	}

	bool Coll::Idlist::append( unsigned int id )
	{
		return xmmsc_coll_idlist_append( coll_.coll_, id );
	}

	bool Coll::Idlist::insert( unsigned int id, unsigned int index )
	{
		return xmmsc_coll_idlist_insert( coll_.coll_, id, index );
	}

	bool Coll::Idlist::move( unsigned int index, unsigned int newindex )
	{
		return xmmsc_coll_idlist_move( coll_.coll_, index, newindex );
	}

	bool Coll::Idlist::remove( unsigned int index )
	{
		return xmmsc_coll_idlist_remove( coll_.coll_, index );
	}

	bool Coll::Idlist::clear()
	{
		return xmmsc_coll_idlist_clear( coll_.coll_ );
	}

	unsigned int Coll::Idlist::size() const
	{
		return xmmsc_coll_idlist_get_size( coll_.coll_ );
	}

	// get/set value at index
	Coll::Idlist::IdlistElement Coll::Idlist::operator []( unsigned int index )
	{
		return IdlistElement( coll_, index );
	}



	Coll::AttributeElement::AttributeElement( Coll& coll, std::string index )
		: AbstractElement< std::string, std::string >( coll, index )
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


	Coll::Idlist::IdlistElement::IdlistElement( Coll& coll, unsigned int index )
		: AbstractElement< unsigned int, unsigned int >( coll, index )
	{
	}

	Coll::Idlist::IdlistElement::operator unsigned int() const
	{
		return coll_.idlistGetIndex( index_ );
	}

	unsigned int
	Coll::Idlist::IdlistElement::operator=( unsigned int value )
	{
		coll_.idlistSetIndex( index_, value );
		return value;
	}

}
