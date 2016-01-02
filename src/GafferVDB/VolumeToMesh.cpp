//////////////////////////////////////////////////////////////////////////
//
//  Copyright (c) 2016, John Haddon. All rights reserved.
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
#include "openvdb/tools/VolumeToMesh.h"

#include "IECore/MeshPrimitive.h"

#include "GafferVDB/VDBObject.h"
#include "GafferVDB/VolumeToMesh.h"

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

IECore::MeshPrimitivePtr volumeToMesh( openvdb::GridBase::ConstPtr grid, double isoValue, double adaptivity )
{
	openvdb::tools::VolumeToMesh mesher( isoValue, adaptivity );

	/// \todo PROPER CHECKING, DEALING WITH OTHER TYPES
	mesher( *openvdb::gridConstPtrCast<openvdb::FloatGrid>( grid ) );

	// Copy out topology

	IntVectorDataPtr verticesPerFaceData = new IntVectorData;
	vector<int> &verticesPerFace = verticesPerFaceData->writable();

	IntVectorDataPtr vertexIdsData = new IntVectorData;
	vector<int> &vertexIds = vertexIdsData->writable();

	/// \todo PREALLOCATE

	for( size_t i = 0, n = mesher.polygonPoolListSize(); i < n; ++i )
	{
		const openvdb::tools::PolygonPool &polygonPool = mesher.polygonPoolList()[i];
		for( size_t qi = 0, qn = polygonPool.numQuads(); qi < qn; ++qi )
		{
			openvdb::math::Vec4ui quad = polygonPool.quad( qi );
			verticesPerFace.push_back( 4 );
			vertexIds.push_back( quad[0] );
			vertexIds.push_back( quad[1] );
			vertexIds.push_back( quad[2] );
			vertexIds.push_back( quad[3] );
		}

		for( size_t ti = 0, tn = polygonPool.numTriangles(); ti < tn; ++ti )
		{
			openvdb::math::Vec3ui triangle = polygonPool.triangle( ti );
			verticesPerFace.push_back( 3 );
			vertexIds.push_back( triangle[0] );
			vertexIds.push_back( triangle[1] );
			vertexIds.push_back( triangle[2] );
		}
	}

	// Copy out points

	V3fVectorDataPtr pointsData = new V3fVectorData;
	vector<V3f> &points = pointsData->writable();

	points.reserve( mesher.pointListSize() );
	for( size_t i = 0, n = mesher.pointListSize(); i < n; ++i )
	{
		const openvdb::math::Vec3s v = mesher.pointList()[i];
		points.push_back( V3f( v.x(), v.y(), v.z() ) );
	}

	return new MeshPrimitive( verticesPerFaceData, vertexIdsData, "linear", pointsData );
}

/*volumeToMesh(
    const GridType& grid,
    std::vector<Vec3s>& points,
    std::vector<Vec3I>& triangles,
    std::vector<Vec4I>& quads,
    double isovalue,
    double adaptivity)
{
    VolumeToMesh mesher(isovalue, adaptivity);
    mesher(grid);

    // Preallocate the point list
    points.clear();
    points.resize(mesher.pointListSize());

    { // Copy points
        internal::PointListCopy ptnCpy(mesher.pointList(), points);
        tbb::parallel_for(tbb::blocked_range<size_t>(0, points.size()), ptnCpy);
        mesher.pointList().reset(NULL);
    }

    PolygonPoolList& polygonPoolList = mesher.polygonPoolList();

    { // Preallocate primitive lists
        size_t numQuads = 0, numTriangles = 0;
        for (size_t n = 0, N = mesher.polygonPoolListSize(); n < N; ++n) {
            openvdb::tools::PolygonPool& polygons = polygonPoolList[n];
            numTriangles += polygons.numTriangles();
            numQuads += polygons.numQuads();
        }

        triangles.clear();
        triangles.resize(numTriangles);
        quads.clear();
        quads.resize(numQuads);
    }

    // Copy primitives
    size_t qIdx = 0, tIdx = 0;
    for (size_t n = 0, N = mesher.polygonPoolListSize(); n < N; ++n) {
        openvdb::tools::PolygonPool& polygons = polygonPoolList[n];

        for (size_t i = 0, I = polygons.numQuads(); i < I; ++i) {
            quads[qIdx++] = polygons.quad(i);
        }

        for (size_t i = 0, I = polygons.numTriangles(); i < I; ++i) {
            triangles[tIdx++] = polygons.triangle(i);
        }
    }
}*/

} // namespace

//////////////////////////////////////////////////////////////////////////
// VolumeToMesh implementation
//////////////////////////////////////////////////////////////////////////

IE_CORE_DEFINERUNTIMETYPED( VolumeToMesh );

size_t VolumeToMesh::g_firstPlugIndex = 0;

VolumeToMesh::VolumeToMesh( const std::string &name )
	:	SceneElementProcessor( name )
{
	storeIndexOfNextChild( g_firstPlugIndex );

	addChild( new FloatPlug( "isoValue", Plug::In, 0.0f ) );
	addChild( new FloatPlug( "adaptivity", Plug::In, 0.0f, 0.0f, 1.0f ) );
}

VolumeToMesh::~VolumeToMesh()
{
}

Gaffer::FloatPlug *VolumeToMesh::isoValuePlug()
{
	return getChild<FloatPlug>( g_firstPlugIndex );
}

const Gaffer::FloatPlug *VolumeToMesh::isoValuePlug() const
{
	return getChild<FloatPlug>( g_firstPlugIndex );
}

Gaffer::FloatPlug *VolumeToMesh::adaptivityPlug()
{
	return getChild<FloatPlug>( g_firstPlugIndex + 1 );
}

const Gaffer::FloatPlug *VolumeToMesh::adaptivityPlug() const
{
	return getChild<FloatPlug>( g_firstPlugIndex + 1 );
}

void VolumeToMesh::affects( const Gaffer::Plug *input, AffectedPlugsContainer &outputs ) const
{
	SceneElementProcessor::affects( input, outputs );

	if(
		input == isoValuePlug() ||
		input == adaptivityPlug()
	)
	{
		outputs.push_back( outPlug()->objectPlug() );
	}
}

bool VolumeToMesh::processesObject() const
{
	return true;
}

void VolumeToMesh::hashProcessedObject( const ScenePath &path, const Gaffer::Context *context, IECore::MurmurHash &h ) const
{
	SceneElementProcessor::hashProcessedObject( path, context, h );

	isoValuePlug()->hash( h );
	adaptivityPlug()->hash( h );
}

IECore::ConstObjectPtr VolumeToMesh::computeProcessedObject( const ScenePath &path, const Gaffer::Context *context, IECore::ConstObjectPtr inputObject ) const
{
	const VDBObject *vdbObject = runTimeCast<const VDBObject>( inputObject.get() );
	if( !vdbObject )
	{
		return inputObject;
	}

	return volumeToMesh( vdbObject->grid(), isoValuePlug()->getValue(), adaptivityPlug()->getValue() );
}
