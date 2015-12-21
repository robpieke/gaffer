//////////////////////////////////////////////////////////////////////////
//
//  Copyright (c) 2015, John Haddon. All rights reserved.
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

#include "GafferVDB/MeshToVDB.h"

using namespace std;
using namespace IECore;
using namespace Gaffer;
using namespace GafferVDB;

//////////////////////////////////////////////////////////////////////////
// Utilities. Perhaps these belong in Cortex one day?
//////////////////////////////////////////////////////////////////////////

namespace
{

struct CortexMeshDataAdapter
{

	CortexMeshDataAdapter( const IECore::MeshPrimitive *mesh )
		:	m_numFaces( mesh->numFaces() ),
			m_numVertices( mesh->variableSize( PrimitiveVariable::Vertex ) ),
			m_verticesPerFace( mesh->verticesPerFace()->readable() ),
			m_vertexIds( mesh->vertexIds()->readable() )
	{
		size_t offset = 0;
		m_polygonOffsets.reserve( m_polygonCount );
		for( vector<int>::const_iterator it = m_verticesPerFace.begin(), eIt = m_verticesPerFace.end(); it != eIt; ++it )
		{
			m_faceOffsets.push_back( offset );
			offset += *it;
		}
	}

	size_t polygonCount() const
	{
		return m_polygonCount;
	}

	size_t pointCount() const
	{
		return m_pointCount;
	}

	size_t vertexCount( size_t polygonIndex ) const
	{
		return m_verticesPerFace[polygonIndex];
	}

	/// NEED TO WORRY ABOUT THE "GRID INDEX SPACE PART!!"
	// Return position pos in local grid index space for polygon n and vertex v
	void getIndexSpacePoint( size_t polygonIndex, size_t polygonVertexIndex, openvdb::Vec3d &pos ) const
	{
		!!!
	}

	private :

		const size_t m_numFaces;
		const size_t m_numVertices;
		const vector<int> &m_verticesPerFace;
		const vector<int> &m_vertexIds;
		vector<int> m_faceOffsets;

};

} // namespace

//////////////////////////////////////////////////////////////////////////
// MeshToVDB implementation
//////////////////////////////////////////////////////////////////////////

IE_CORE_DEFINERUNTIMETYPED( MeshToVDB );

size_t MeshToVDB::g_firstPlugIndex = 0;

MeshToVDB::MeshToVDB( const std::string &name )
	:	SceneElementProcessor( name )
{
	storeIndexOfNextChild( g_firstPlugIndex );
}

MeshToVDB::~MeshToVDB()
{
}

void MeshToVDB::affects( const Gaffer::Plug *input, AffectedPlugsContainer &outputs ) const
{
	SceneElementProcessor::affects( input, outputs );
}

bool MeshToVDB::processesObject() const
{
	return true;
}

void MeshToVDB::hashProcessedObject( const ScenePath &path, const Gaffer::Context *context, IECore::MurmurHash &h ) const
{
	//SceneElementProcessor::hashProcessedObject( path, context, h );
}

IECore::ConstObjectPtr MeshToVDB::computeProcessedObject( const ScenePath &path, const Gaffer::Context *context, IECore::ConstObjectPtr inputObject ) const
{
	const MeshPrimitive *mesh = runTimeCast<MeshPrimitive>( inputObject.get() );
	if( !mesh )
	{
		return inputObject;
	}


}
