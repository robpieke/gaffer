//////////////////////////////////////////////////////////////////////////
//
//  Copyright (c) 2018, John Haddon. All rights reserved.
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

#include "SceneGraphAlgo.h"

#include "IECoreScene/MeshPrimitive.h"

#include "IECore/DataAlgo.h"
#include "IECore/MessageHandler.h"

#include "RixPredefinedStrings.hpp"

using namespace std;
using namespace IECore;
using namespace IECoreScene;
using namespace IECoreRenderMan;

namespace
{

RixDetailType detail( IECoreScene::PrimitiveVariable::Interpolation interpolation )
{
	switch( interpolation )
	{
		case PrimitiveVariable::Invalid :
			throw IECore::Exception( "No detail equivalent to PrimitiveVariable::Invalid" );
		case PrimitiveVariable::Constant :
			return RixDetailType::k_constant;
		case PrimitiveVariable::Uniform :
			return RixDetailType::k_uniform;
		case PrimitiveVariable::Vertex :
			return RixDetailType::k_vertex;
		case PrimitiveVariable::Varying :
			return RixDetailType::k_varying;
		case PrimitiveVariable::FaceVarying :
			return RixDetailType::k_facevarying;
	}
}

RixDataType dataType( IECore::GeometricData::Interpretation interpretation )
{
	switch( interpretation )
	{
		case GeometricData::Vector :
			return RixDataType::k_vector;
		case GeometricData::Normal :
			return RixDataType::k_normal;
		default :
			return RixDataType::k_point;
	}
}

struct PrimitiveVariableEmitter
{

	void operator()( const V3fVectorData *data, const PrimitiveVariableMap::value_type &primitiveVariable, RixParamList *paramList ) const
	{
		emit(
			data,
			{
				RtUString( primitiveVariable.first.c_str() ),
				dataType( data->getInterpretation() ),
				/* length = */ 1,
				detail( primitiveVariable.second.interpolation ),
				/* array = */ false,
			},
			primitiveVariable,
			paramList
		);
	}

	void operator()( const V2fVectorData *data, const PrimitiveVariableMap::value_type &primitiveVariable, RixParamList *paramList ) const
	{
		emit(
			data,
			{
				RtUString( primitiveVariable.first.c_str() ),
				RixDataType::k_float,
				/* length = */ 2,
				detail( primitiveVariable.second.interpolation ),
				/* array = */ true,
			},
			primitiveVariable,
			paramList
		);
	}

	void operator()( const Data *data, const PrimitiveVariableMap::value_type &primitiveVariable, RixParamList *paramList ) const
	{
		IECore::msg(
			IECore::Msg::Warning,
			"IECoreRenderMan",
			boost::format( "Unsupported primitive variable of type \"%s\"" ) % data->typeName()
		);
	}

	private :

		template<typename T>
		void emit( const T *data, const RixParamList::ParamInfo &paramInfo, const PrimitiveVariableMap::value_type &primitiveVariable, RixParamList *paramList ) const
		{
			if( primitiveVariable.second.indices )
			{
				typedef RixParamList::Buffer<typename T::ValueType::value_type> Buffer;
				Buffer buffer( *paramList, paramInfo, /* time = */ 0 );
				buffer.Bind();

				const vector<int> &indices = primitiveVariable.second.indices->readable();
				const typename T::ValueType &values = data->readable();
				for( int i = 0, e = indices.size(); i < e; ++i )
				{
					buffer[i] = values[indices[i]];
				}

				buffer.Unbind();
			}
			else
			{
				paramList->SetParam(
					paramInfo,
					data->readable().data(),
					0
				);
			}
		}

};

RixSGGroup *convertStatic( const IECoreScene::MeshPrimitive *mesh, RixSGScene *scene, RtUString identifier )
{
	RixSGMesh *result = scene->CreateMesh( identifier );
	result->Define(
		mesh->variableSize( PrimitiveVariable::Uniform ),
		mesh->variableSize( PrimitiveVariable::Vertex ),
		mesh->variableSize( PrimitiveVariable::FaceVarying )
	);

	RixParamList *primVars = result->EditPrimVarBegin();

	primVars->SetIntegerDetail( Rix::k_Ri_nvertices, mesh->verticesPerFace()->readable().data(), RixDetailType::k_uniform );
	primVars->SetIntegerDetail( Rix::k_Ri_vertices, mesh->vertexIds()->readable().data(), RixDetailType::k_facevarying );

	for( auto &primitiveVariable : mesh->variables )
	{
		dispatch( primitiveVariable.second.data.get(), PrimitiveVariableEmitter(), primitiveVariable, primVars );
	}

	result->EditPrimVarEnd( primVars );
	return result;
}

SceneGraphAlgo::ConverterDescription<MeshPrimitive> g_description( convertStatic );

} // namespace
