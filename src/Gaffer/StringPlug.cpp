//////////////////////////////////////////////////////////////////////////
//
//  Copyright (c) 2011-2012, John Haddon. All rights reserved.
//  Copyright (c) 2011-2015, Image Engine Design Inc. All rights reserved.
//
//  Redistribution and use in source and binary forms, with or without
//  modification, are permitted provided that the following conditions are
//  met:
//
//      * Redistributions of source code must retain the above
//        copyright notice, this list of conditions and the following
//        disclaimer.
//
//      * Redistributions in binary form must reproduce the above
//        copyright notice, this list of conditions and the following
//        disclaimer in the documentation and/or other materials provided with
//        the distribution.
//
//      * Neither the name of John Haddon nor the names of
//        any other contributors to this software may be used to endorse or
//        promote products derived from this software without specific prior
//        written permission.
//
//  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
//  IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
//  THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
//  PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
//  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
//  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
//  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
//  PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
//  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
//  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
//  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
//////////////////////////////////////////////////////////////////////////

#include "Gaffer/Context.h"
#include "Gaffer/StringPlug.h"

using namespace IECore;
using namespace Gaffer;

namespace
{

// Custom StringData derived class used to store
// additional information about the value. We are
// deliberately omitting a custom TypeId etc because
// this is just a private class.
/// \todo At some point we might also want to optionally
/// store an InternedString and have a StringPlug::getInternedValue()
/// method for those clients that could use it.
class ValueData : public IECore::StringData
{

	public :

		ValueData( const std::string &value )
			:	StringData( value ), hasSubstitutions( Context::hasSubstitutions( value ) )
		{
		}

		bool hasSubstitutions;

	protected :

		virtual void copyFrom( const Object *other, CopyContext *context )
		{
			StringData::copyFrom( other, context );
			hasSubstitutions = static_cast<const ValueData *>( other )->hasSubstitutions;
		}

		virtual void load( LoadContextPtr context )
		{
			Data::load( context );
			hasSubstitutions = Context::hasSubstitutions( readable() );
		}

};

} // namespace

IE_CORE_DEFINERUNTIMETYPED( StringPlug );

StringPlug::StringPlug(
	const std::string &name,
	Direction direction,
	const std::string &defaultValue,
	unsigned flags,
	unsigned substitutions
)
	:	ValuePlug( name, direction, new ValueData( defaultValue ), flags ), m_substitutions( substitutions )
{
}

StringPlug::~StringPlug()
{
}

unsigned StringPlug::substitutions() const
{
	return m_substitutions;
}

bool StringPlug::acceptsInput( const Plug *input ) const
{
	if( !ValuePlug::acceptsInput( input ) )
	{
		return false;
	}
	if( input )
	{
		return input->isInstanceOf( staticTypeId() );
	}
	return true;
}

PlugPtr StringPlug::createCounterpart( const std::string &name, Direction direction ) const
{
	return new StringPlug( name, direction, defaultValue(), getFlags(), substitutions() );
}

const std::string &StringPlug::defaultValue() const
{
	return static_cast<const ValueData *>( defaultObjectValue() )->readable();
}

void StringPlug::setValue( const std::string &value )
{
	setObjectValue( new ValueData( value ) );
}

std::string StringPlug::getValue( const IECore::MurmurHash *precomputedHash ) const
{
	IECore::ConstObjectPtr o = getObjectValue( precomputedHash );
	const ValueData *s = static_cast<const ValueData *>( o.get() );

	bool performSubstitution =
		m_substitutions &&
		direction()==Plug::In &&
		inCompute() &&
		Plug::getFlags( Plug::PerformsSubstitutions ) &&
		s->hasSubstitutions
	;

	return performSubstitution ? Context::current()->substitute( s->readable(), m_substitutions ) : s->readable();
}

void StringPlug::setFrom( const ValuePlug *other )
{
	const StringPlug *tOther = IECore::runTimeCast<const StringPlug >( other );
	if( tOther )
	{
		setValue( tOther->getValue() );
	}
	else
	{
		throw IECore::Exception( "Unsupported plug type" );
	}
}

IECore::MurmurHash StringPlug::hash() const
{
	const StringPlug *p = source<StringPlug>();

	const bool performSubstitution = m_substitutions && p->direction()==Plug::In && p->getFlags( Plug::PerformsSubstitutions );
	if( performSubstitution )
	{
		IECore::ConstObjectPtr o = p->getObjectValue();
		const ValueData *s = static_cast<const ValueData *>( o.get() );

		if( s->hasSubstitutions )
		{
			IECore::MurmurHash result;
			result.append( Context::current()->substitute( s->readable(), m_substitutions ) );
			return result;
		}
	}

	// no substitutions
	return p->ValuePlug::hash();
}
