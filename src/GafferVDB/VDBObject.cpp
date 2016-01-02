//////////////////////////////////////////////////////////////////////////
//
//  Copyright (c) 2015, John Haddon. All rights reserved.
//
//  Redistribution and use in source and binary forms, with or without
//  modification, are permitted provided that the following conditions are
//  met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in the
//       documentation and/or other materials provided with the distribution.
//
//     * Neither the name of Image Engine Design nor the names of any
//       other contributors to this software may be used to endorse or
//       promote products derived from this software without specific prior
//       written permission.
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

#include "IECore/Exception.h"

#include "GafferVDB/VDBObject.h"

using namespace IECore;
using namespace GafferVDB;

IE_CORE_DEFINEOBJECTTYPEDESCRIPTION( VDBObject );

const unsigned int VDBObject::m_ioVersion = 0;

VDBObject::VDBObject( openvdb::GridBase::Ptr grid )
	:	m_grid( grid )
{
}

VDBObject::~VDBObject()
{
}

openvdb::GridBase::Ptr VDBObject::grid()
{
	return m_grid;
}

openvdb::GridBase::ConstPtr VDBObject::grid() const
{
	return m_grid;
}

void VDBObject::copyFrom( const Object *other, CopyContext *context )
{
	Object::copyFrom( other, context );
	const VDBObject *otherVDBObject = static_cast<const VDBObject *>( other );
	if( !otherVDBObject->m_grid )
	{
		m_grid.reset();
	}
	else
	{
		m_grid = otherVDBObject->m_grid->deepCopyGrid();
	}
}

bool VDBObject::isEqualTo( const Object *other ) const
{
	if( !Object::isEqualTo( other ) )
	{
		return false;
	}

	const VDBObject *otherVDBObject = static_cast<const VDBObject *>( other );
	return m_grid == otherVDBObject->m_grid;
}

void VDBObject::save( SaveContext *context ) const
{
	Object::save( context );
	throw IECore::NotImplementedException( "VDBObject::save" );
}

void VDBObject::load( LoadContextPtr context )
{
	Object::load( context );
	throw IECore::NotImplementedException( "VDBObject::load" );
}

void VDBObject::memoryUsage( Object::MemoryAccumulator &a ) const
{
	Object::memoryUsage( a );
	a.accumulate( m_grid->memUsage() );
}

void VDBObject::hash( MurmurHash &h ) const
{
	Object::hash( h );
	throw IECore::NotImplementedException( "VDBObject::hash" );
}
