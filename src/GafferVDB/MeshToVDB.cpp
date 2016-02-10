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

#include "openvdb/openvdb.h"
#include "openvdb/tools/MeshToVolume.h"

#include "IECore/MeshPrimitive.h"

#include "GafferVDB/VDBObject.h"
#include "GafferVDB/MeshToVDB.h"

using namespace std;
using namespace Imath;
using namespace IECore;
using namespace Gaffer;
using namespace GafferVDB;

//////////////////////////////////////////////////////////////////////////
// Utilities. Perhaps these belong in Cortex one day?
//////////////////////////////////////////////////////////////////////////

namespace
{

struct CortexMeshAdapter
{

	CortexMeshAdapter( const IECore::MeshPrimitive *mesh, const openvdb::math::Transform *transform )
		:	m_numFaces( mesh->numFaces() ),
			m_numVertices( mesh->variableSize( PrimitiveVariable::Vertex ) ),
			m_verticesPerFace( mesh->verticesPerFace()->readable() ),
			m_vertexIds( mesh->vertexIds()->readable() ),
			m_transform( transform )
	{
		size_t offset = 0;
		m_faceOffsets.reserve( m_numFaces );
		for( vector<int>::const_iterator it = m_verticesPerFace.begin(), eIt = m_verticesPerFace.end(); it != eIt; ++it )
		{
			m_faceOffsets.push_back( offset );
			offset += *it;
		}

		const V3fVectorData *points = mesh->variableData<V3fVectorData>( "P", PrimitiveVariable::Vertex );
		m_points = &points->readable();
	}

	size_t polygonCount() const
	{
		return m_numFaces;
	}

	size_t pointCount() const
	{
		return m_numVertices;
	}

	size_t vertexCount( size_t polygonIndex ) const
	{
		return m_verticesPerFace[polygonIndex];
	}

	// Return position pos in local grid index space for polygon n and vertex v
	void getIndexSpacePoint( size_t polygonIndex, size_t polygonVertexIndex, openvdb::Vec3d &pos ) const
	{
		/// \todo Threaded pretransform in constructor?
		const V3f p = (*m_points)[ m_vertexIds[ m_faceOffsets[polygonIndex] + polygonVertexIndex ] ];
		pos = m_transform->worldToIndex( openvdb::math::Vec3s( p.x, p.y, p.z ) );
	}

	private :

	/// \todo WE DON'T NEED ALL THIS CRAP
		const size_t m_numFaces;
		const size_t m_numVertices;
		const vector<int> &m_verticesPerFace;
		const vector<int> &m_vertexIds;
		vector<int> m_faceOffsets;
		const vector<V3f> *m_points;
		const openvdb::math::Transform *m_transform;

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

	addChild( new FloatPlug( "voxelSize", Plug::In, 0.1f, 0.0001f ) );
}

MeshToVDB::~MeshToVDB()
{
}

FloatPlug *MeshToVDB::voxelSizePlug()
{
	return getChild<FloatPlug>( g_firstPlugIndex );
}

const FloatPlug *MeshToVDB::voxelSizePlug() const
{
	return getChild<FloatPlug>( g_firstPlugIndex );
}

void MeshToVDB::affects( const Gaffer::Plug *input, AffectedPlugsContainer &outputs ) const
{
	SceneElementProcessor::affects( input, outputs );

	if( input == voxelSizePlug() )
	{
		outputs.push_back( outPlug()->objectPlug() );
	}
}

bool MeshToVDB::processesObject() const
{
	return true;
}

void MeshToVDB::hashProcessedObject( const ScenePath &path, const Gaffer::Context *context, IECore::MurmurHash &h ) const
{
	SceneElementProcessor::hashProcessedObject( path, context, h );

	voxelSizePlug()->hash( h );
}

IECore::ConstObjectPtr MeshToVDB::computeProcessedObject( const ScenePath &path, const Gaffer::Context *context, IECore::ConstObjectPtr inputObject ) const
{
	const MeshPrimitive *mesh = runTimeCast<const MeshPrimitive>( inputObject.get() );
	if( !mesh )
	{
		return inputObject;
	}

	const float voxelSize = voxelSizePlug()->getValue();
	openvdb::math::Transform::Ptr transform = openvdb::math::Transform::createLinearTransform( voxelSize );

	openvdb::FloatGrid::Ptr grid = openvdb::tools::meshToVolume<openvdb::FloatGrid>(
		CortexMeshAdapter( mesh, transform.get() ),
		*transform,
		3.0f, //exBand in voxel units, PLUGS PLEASE!!!
		3.0f, //inBand in voxel units,
		0 //conversionFlags,
		//primitiveIndexGrid.get()
	);

	return new VDBObject( grid );
}


	/*doMeshConversion(
    const openvdb::math::Transform& xform,
    const std::vector<Vec3s>& points,
    const std::vector<Vec3I>& triangles,
    const std::vector<Vec4I>& quads,
    float exBandWidth,
    float inBandWidth,
    bool unsignedDistanceField = false)
{
    if (points.empty()) {
        return typename GridType::Ptr(new GridType(typename GridType::ValueType(exBandWidth)));
    }

    const size_t numPoints = points.size();
    boost::scoped_array<Vec3s> indexSpacePoints(new Vec3s[numPoints]);

    // transform points to local grid index space
    tbb::parallel_for(tbb::blocked_range<size_t>(0, numPoints),
        mesh_to_volume_internal::TransformPoints<Vec3s>(
                &points[0], indexSpacePoints.get(), xform));

    const int conversionFlags = unsignedDistanceField ? UNSIGNED_DISTANCE_FIELD : 0;

    if (quads.empty()) {

        QuadAndTriangleDataAdapter<Vec3s, Vec3I>
            mesh(indexSpacePoints.get(), numPoints, &triangles[0], triangles.size());

        return meshToVolume<GridType>(mesh, xform, exBandWidth, inBandWidth, conversionFlags);

    } else if (triangles.empty()) {

        QuadAndTriangleDataAdapter<Vec3s, Vec4I>
            mesh(indexSpacePoints.get(), numPoints, &quads[0], quads.size());

        return meshToVolume<GridType>(mesh, xform, exBandWidth, inBandWidth, conversionFlags);
    }

    // pack primitives

    const size_t numPrimitives = triangles.size() + quads.size();
    boost::scoped_array<Vec4I> prims(new Vec4I[numPrimitives]);

    for (size_t n = 0, N = triangles.size(); n < N; ++n) {
        const Vec3I& triangle = triangles[n];
        Vec4I& prim = prims[n];
        prim[0] = triangle[0];
        prim[1] = triangle[1];
        prim[2] = triangle[2];
        prim[3] = util::INVALID_IDX;
    }

    const size_t offset = triangles.size();
    for (size_t n = 0, N = quads.size(); n < N; ++n) {
        prims[offset + n] = quads[n];
    }

    QuadAndTriangleDataAdapter<Vec3s, Vec4I>
        mesh(indexSpacePoints.get(), numPoints, prims.get(), numPrimitives);

    return meshToVolume<GridType>(mesh, xform, exBandWidth, inB*/
