//////////////////////////////////////////////////////////////////////////
//
//  Copyright (c) 2012, John Haddon. All rights reserved.
//  Copyright (c) 2013, Image Engine Design Inc. All rights reserved.
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

#include "GafferScene/Instancer.h"

#include "Gaffer/Context.h"
#include "Gaffer/StringPlug.h"

#include "IECoreScene/Primitive.h"

#include "IECore/DateTimeData.h"
#include "IECore/MessageHandler.h"
#include "IECore/NullObject.h"
#include "IECore/SplineData.h"
#include "IECore/TransformationMatrixData.h"
#include "IECore/VectorTypedData.h"

#include "boost/lexical_cast.hpp"

#include "tbb/blocked_range.h"
#include "tbb/parallel_reduce.h"

#include <functional>

using namespace std;
using namespace std::placeholders;
using namespace tbb;
using namespace Imath;
using namespace IECore;
using namespace IECoreScene;
using namespace Gaffer;
using namespace GafferScene;

//////////////////////////////////////////////////////////////////////////
// Internal utilities
//////////////////////////////////////////////////////////////////////////

namespace
{

// Prototype for a new `IECore::DataAlgo::dispatch()` method to replace
// `IECore::despatchTypedData()`. The latter baffles me every time I come
// to use it, what with its multiple overloads and Enablers and ErrorHandlers.
// This is based on a much simpler principle : downcast to the right type
// and call the functor. We can then use the familiar C++ overload resolution
// rules to do the work of enablers and error handlers in the functor itself.
/// \todo Provide const version.
template<class Functor>
typename std::result_of<Functor( Data * )>::type dispatch( Data *data, Functor &functor )
{
	switch( data->typeId() )
	{
		case BoolDataTypeId :
			return functor( static_cast<BoolData *>( data ) );
		case FloatDataTypeId :
			return functor( static_cast<FloatData *>( data ) );
		case DoubleDataTypeId :
			return functor( static_cast<DoubleData *>( data ) );
		case IntDataTypeId :
			return functor( static_cast<IntData *>( data ) );
		case UIntDataTypeId :
			return functor( static_cast<UIntData *>( data ) );
		case CharDataTypeId :
			return functor( static_cast<CharData *>( data ) );
		case UCharDataTypeId :
			return functor( static_cast<UCharData *>( data ) );
		case ShortDataTypeId :
			return functor( static_cast<ShortData *>( data ) );
		case UShortDataTypeId :
			return functor( static_cast<UShortData *>( data ) );
		case Int64DataTypeId :
			return functor( static_cast<Int64Data *>( data ) );
		case UInt64DataTypeId :
			return functor( static_cast<UInt64Data *>( data ) );
		case StringDataTypeId :
			return functor( static_cast<StringData *>( data ) );
		case InternedStringDataTypeId :
			return functor( static_cast<InternedStringData *>( data ) );
		case HalfDataTypeId :
			return functor( static_cast<HalfData *>( data ) );
		case V2iDataTypeId :
			return functor( static_cast<V2iData *>( data ) );
		case V3iDataTypeId :
			return functor( static_cast<V3iData *>( data ) );
		case V2fDataTypeId :
			return functor( static_cast<V2fData *>( data ) );
		case V3fDataTypeId :
			return functor( static_cast<V3fData *>( data ) );
		case V2dDataTypeId :
			return functor( static_cast<V2dData *>( data ) );
		case V3dDataTypeId :
			return functor( static_cast<V3dData *>( data ) );
		case Color3fDataTypeId :
			return functor( static_cast<Color3fData *>( data ) );
		case Color4fDataTypeId :
			return functor( static_cast<Color4fData *>( data ) );
		case Box2iDataTypeId :
			return functor( static_cast<Box2iData *>( data ) );
		case Box2fDataTypeId :
			return functor( static_cast<Box2fData *>( data ) );
		case Box3fDataTypeId :
			return functor( static_cast<Box3fData *>( data ) );
		case Box2dDataTypeId :
			return functor( static_cast<Box2dData *>( data ) );
		case Box3dDataTypeId :
			return functor( static_cast<Box3dData *>( data ) );
		case M33fDataTypeId :
			return functor( static_cast<M33fData *>( data ) );
		case M33dDataTypeId :
			return functor( static_cast<M33dData *>( data ) );
		case M44fDataTypeId :
			return functor( static_cast<M44fData *>( data ) );
		case M44dDataTypeId :
			return functor( static_cast<M44dData *>( data ) );
		case TransformationMatrixfDataTypeId :
			return functor( static_cast<TransformationMatrixfData *>( data ) );
		case TransformationMatrixdDataTypeId :
			return functor( static_cast<TransformationMatrixdData *>( data ) );
		case QuatfDataTypeId :
			return functor( static_cast<QuatfData *>( data ) );
		case QuatdDataTypeId :
			return functor( static_cast<QuatdData *>( data ) );
		case SplineffDataTypeId :
			return functor( static_cast<SplineffData *>( data ) );
		case SplineddDataTypeId :
			return functor( static_cast<SplineddData *>( data ) );
		case SplinefColor3fDataTypeId :
			return functor( static_cast<SplinefColor3fData *>( data ) );
		case SplinefColor4fDataTypeId :
			return functor( static_cast<SplinefColor4fData *>( data ) );
		case DateTimeDataTypeId :
			return functor( static_cast<DateTimeData *>( data ) );
		case BoolVectorDataTypeId :
			return functor( static_cast<BoolVectorData *>( data ) );
		case FloatVectorDataTypeId :
			return functor( static_cast<FloatVectorData *>( data ) );
		case DoubleVectorDataTypeId :
			return functor( static_cast<DoubleVectorData *>( data ) );
		case HalfVectorDataTypeId :
			return functor( static_cast<HalfVectorData *>( data ) );
		case IntVectorDataTypeId :
			return functor( static_cast<IntVectorData *>( data ) );
		case UIntVectorDataTypeId :
			return functor( static_cast<UIntVectorData *>( data ) );
		case CharVectorDataTypeId :
			return functor( static_cast<CharVectorData *>( data ) );
		case UCharVectorDataTypeId :
			return functor( static_cast<UCharVectorData *>( data ) );
		case ShortVectorDataTypeId :
			return functor( static_cast<ShortVectorData *>( data ) );
		case UShortVectorDataTypeId :
			return functor( static_cast<UShortVectorData *>( data ) );
		case Int64VectorDataTypeId :
			return functor( static_cast<Int64VectorData *>( data ) );
		case UInt64VectorDataTypeId :
			return functor( static_cast<UInt64VectorData *>( data ) );
		case StringVectorDataTypeId :
			return functor( static_cast<StringVectorData *>( data ) );
		case InternedStringVectorDataTypeId :
			return functor( static_cast<InternedStringVectorData *>( data ) );
		case V2iVectorDataTypeId :
			return functor( static_cast<V2iVectorData *>( data ) );
		case V2fVectorDataTypeId :
			return functor( static_cast<V2fVectorData *>( data ) );
		case V2dVectorDataTypeId :
			return functor( static_cast<V2dVectorData *>( data ) );
		case V3iVectorDataTypeId :
			return functor( static_cast<V3iVectorData *>( data ) );
		case V3fVectorDataTypeId :
			return functor( static_cast<V3fVectorData *>( data ) );
		case V3dVectorDataTypeId :
			return functor( static_cast<V3dVectorData *>( data ) );
		case Box3fVectorDataTypeId :
			return functor( static_cast<Box3fVectorData *>( data ) );
		case Box3dVectorDataTypeId :
			return functor( static_cast<Box3dVectorData *>( data ) );
		case M33fVectorDataTypeId :
			return functor( static_cast<M33fVectorData *>( data ) );
		case M33dVectorDataTypeId :
			return functor( static_cast<M33dVectorData *>( data ) );
		case M44fVectorDataTypeId :
			return functor( static_cast<M44fVectorData *>( data ) );
		case M44dVectorDataTypeId :
			return functor( static_cast<M44dVectorData *>( data ) );
		case QuatfVectorDataTypeId :
			return functor( static_cast<QuatfVectorData *>( data ) );
		case QuatdVectorDataTypeId :
			return functor( static_cast<QuatdVectorData *>( data ) );
		case Color3fVectorDataTypeId :
			return functor( static_cast<Color3fVectorData *>( data ) );
		case Color4fVectorDataTypeId :
			return functor( static_cast<Color4fVectorData *>( data ) );
		default :
			throw InvalidArgumentException( "Data has unknown type" );
	}
}

template<class Functor>
typename std::result_of<Functor( Data * )>::type dispatch( Data *data )
{
	Functor f;
	return dispatch( data, f );
}

} // namespace

//////////////////////////////////////////////////////////////////////////
// EngineData
//////////////////////////////////////////////////////////////////////////

// Custom Data derived class used to encapsulate the data and
// logic needed to generate instances. We are deliberately omitting
// a custom TypeId etc because this is just a private class.
class Instancer::EngineData : public Data
{

	public :

		EngineData(
			ConstObjectPtr object,
			const std::string &index,
			const std::string &position,
			const std::string &orientation,
			const std::string &scale,
			const std::string &attributes
		)
			:	m_indices( nullptr ),
				m_positions( nullptr ),
				m_orientations( nullptr ),
				m_scales( nullptr ),
				m_uniformScales( nullptr )
		{
			m_primitive = runTimeCast<const Primitive>( object );
			if( !m_primitive )
			{
				return;
			}

			if( const IntVectorData *indices = m_primitive->variableData<IntVectorData>( index ) )
			{
				m_indices = &indices->readable();
				if( m_indices->size() != numInstances() )
				{
					throw IECore::Exception( "Index primitive variable has incorrect size" );
				}
			}

			if( const V3fVectorData *p = m_primitive->variableData<V3fVectorData>( position ) )
			{
				m_positions = &p->readable();
				if( m_positions->size() != numInstances() )
				{
					throw IECore::Exception( "Position primitive variable has incorrect size" );
				}
			}

			if( const QuatfVectorData *o = m_primitive->variableData<QuatfVectorData>( orientation ) )
			{
				m_orientations = &o->readable();
				if( m_orientations->size() != numInstances() )
				{
					throw IECore::Exception( "Orientation primitive variable has incorrect size" );
				}
			}

			if( const V3fVectorData *s = m_primitive->variableData<V3fVectorData>( scale ) )
			{
				m_scales = &s->readable();
				if( m_scales->size() != numInstances() )
				{
					throw IECore::Exception( "Scale primitive variable has incorrect size" );
				}
			}
			else if( const FloatVectorData *s = m_primitive->variableData<FloatVectorData>( scale ) )
			{
				m_uniformScales = &s->readable();
				if( m_uniformScales->size() != numInstances() )
				{
					throw IECore::Exception( "Scale primitive variable has incorrect size" );
				}
			}

			initAttributes( attributes );
		}

		size_t numInstances() const
		{
			return m_primitive ? m_primitive->variableSize( PrimitiveVariable::Vertex ) : 0;
		}

		size_t instanceIndex( size_t instanceIndex ) const
		{
			return m_indices ? (*m_indices)[instanceIndex] : 0;
		}

		M44f instanceTransform( size_t instanceIndex ) const
		{
			M44f result;
			if( m_positions )
			{
				result.translate( (*m_positions)[instanceIndex] );
			}
			if( m_orientations )
			{
				result = (*m_orientations)[instanceIndex].toMatrix44() * result;
			}
			if( m_scales )
			{
				result.scale( (*m_scales)[instanceIndex] );
			}
			if( m_uniformScales )
			{
				result.scale( V3f( (*m_uniformScales)[instanceIndex] ) );
			}
			return result;
		}

		CompoundObjectPtr instanceAttributes( size_t instanceIndex ) const
		{
			if( m_attributeCreators.empty() )
			{
				return nullptr;
			}
			CompoundObjectPtr result = new CompoundObject;
			CompoundObject::ObjectMap &writableResult = result->members();
			for( const auto &attributeCreator : m_attributeCreators )
			{
				writableResult[attributeCreator.first] = attributeCreator.second( instanceIndex );
			}
			return result;
		}

	protected :

		void copyFrom( const Object *other, CopyContext *context ) override
		{
			Data::copyFrom( other, context );
			msg( Msg::Warning, "EngineData::copyFrom", "Not implemented" );
		}

		void save( SaveContext *context ) const override
		{
			Data::save( context );
			msg( Msg::Warning, "EngineData::save", "Not implemented" );
		}

		void load( LoadContextPtr context ) override
		{
			Data::load( context );
			msg( Msg::Warning, "EngineData::load", "Not implemented" );
		}

	private :

		typedef std::function<DataPtr ( size_t )> AttributeCreator;

		struct MakeAttributeCreator
		{

			template<typename T>
			AttributeCreator operator()( const TypedData<vector<T>> *data )
			{
				return std::bind( &createAttribute<T>, data->readable(), ::_1 );
			}

			template<typename T>
			AttributeCreator operator()( const GeometricTypedData<vector<T>> *data )
			{
				return std::bind( &createGeometricAttribute<T>, data->readable(), data->getInterpretation(), ::_1 );
			}

			AttributeCreator operator()( const Data *data )
			{
				throw IECore::InvalidArgumentException( "Expected VectorTypedData" );
			}

			private :

				template<typename T>
				static DataPtr createAttribute( const vector<T> &values, size_t index )
				{
					return new TypedData<T>( values[index] );
				}

				template<typename T>
				static DataPtr createGeometricAttribute( const vector<T> &values, GeometricData::Interpretation interpretation, size_t index )
				{
					return new GeometricTypedData<T>( values[index], interpretation );
				}

		};

		void initAttributes( const std::string &attributes )
		{
			for( auto &primVar : m_primitive->variables )
			{
				if( primVar.second.interpolation != PrimitiveVariable::Vertex )
				{
					continue;
				}
				if( !StringAlgo::matchMultiple( primVar.first, attributes ) )
				{
					continue;
				}
				DataPtr d = primVar.second.expandedData();
				AttributeCreator attributeCreator = dispatch<MakeAttributeCreator>( d.get() );
				m_attributeCreators[primVar.first] = attributeCreator;
			}
		}

		IECoreScene::ConstPrimitivePtr m_primitive;
		const std::vector<int> *m_indices;
		const std::vector<Imath::V3f> *m_positions;
		const std::vector<Imath::Quatf> *m_orientations;
		const std::vector<Imath::V3f> *m_scales;
		const std::vector<float> *m_uniformScales;

		boost::container::flat_map<InternedString, AttributeCreator> m_attributeCreators;

};

//////////////////////////////////////////////////////////////////////////
// Instancer
//////////////////////////////////////////////////////////////////////////

IE_CORE_DEFINERUNTIMETYPED( Instancer );

size_t Instancer::g_firstPlugIndex = 0;

static const IECore::InternedString idContextName( "instancer:id" );

Instancer::Instancer( const std::string &name )
	:	BranchCreator( name )
{
	storeIndexOfNextChild( g_firstPlugIndex );
	addChild( new StringPlug( "name", Plug::In, "instances" ) );
	addChild( new ScenePlug( "instances" ) );
	addChild( new StringPlug( "index", Plug::In, "instanceIndex" ) );
	addChild( new StringPlug( "position", Plug::In, "P" ) );
	addChild( new StringPlug( "orientation", Plug::In ) );
	addChild( new StringPlug( "scale", Plug::In ) );
	addChild( new StringPlug( "attributes", Plug::In ) );
	addChild( new ObjectPlug( "__engine", Plug::Out, NullObject::defaultNullObject() ) );
	addChild( new AtomicCompoundDataPlug( "__instanceChildNames", Plug::Out, new CompoundData ) );
}

Instancer::~Instancer()
{
}

Gaffer::StringPlug *Instancer::namePlug()
{
	return getChild<StringPlug>( g_firstPlugIndex );
}

const Gaffer::StringPlug *Instancer::namePlug() const
{
	return getChild<StringPlug>( g_firstPlugIndex );
}

ScenePlug *Instancer::instancesPlug()
{
	return getChild<ScenePlug>( g_firstPlugIndex + 1 );
}

const ScenePlug *Instancer::instancesPlug() const
{
	return getChild<ScenePlug>( g_firstPlugIndex + 1 );
}

Gaffer::StringPlug *Instancer::indexPlug()
{
	return getChild<StringPlug>( g_firstPlugIndex + 2 );
}

const Gaffer::StringPlug *Instancer::indexPlug() const
{
	return getChild<StringPlug>( g_firstPlugIndex + 2 );
}

Gaffer::StringPlug *Instancer::positionPlug()
{
	return getChild<StringPlug>( g_firstPlugIndex + 3 );
}

const Gaffer::StringPlug *Instancer::positionPlug() const
{
	return getChild<StringPlug>( g_firstPlugIndex + 3 );
}

Gaffer::StringPlug *Instancer::orientationPlug()
{
	return getChild<StringPlug>( g_firstPlugIndex + 4 );
}

const Gaffer::StringPlug *Instancer::orientationPlug() const
{
	return getChild<StringPlug>( g_firstPlugIndex + 4 );
}

Gaffer::StringPlug *Instancer::scalePlug()
{
	return getChild<StringPlug>( g_firstPlugIndex + 5 );
}

const Gaffer::StringPlug *Instancer::scalePlug() const
{
	return getChild<StringPlug>( g_firstPlugIndex + 5 );
}

Gaffer::StringPlug *Instancer::attributesPlug()
{
	return getChild<StringPlug>( g_firstPlugIndex + 6 );
}

const Gaffer::StringPlug *Instancer::attributesPlug() const
{
	return getChild<StringPlug>( g_firstPlugIndex + 6 );
}

Gaffer::ObjectPlug *Instancer::enginePlug()
{
	return getChild<ObjectPlug>( g_firstPlugIndex + 7 );
}

const Gaffer::ObjectPlug *Instancer::enginePlug() const
{
	return getChild<ObjectPlug>( g_firstPlugIndex + 7 );
}

Gaffer::AtomicCompoundDataPlug *Instancer::instanceChildNamesPlug()
{
	return getChild<AtomicCompoundDataPlug>( g_firstPlugIndex + 8 );
}

const Gaffer::AtomicCompoundDataPlug *Instancer::instanceChildNamesPlug() const
{
	return getChild<AtomicCompoundDataPlug>( g_firstPlugIndex + 8 );
}

void Instancer::affects( const Plug *input, AffectedPlugsContainer &outputs ) const
{
	BranchCreator::affects( input, outputs );

	if(
		input == inPlug()->objectPlug() ||
		input == positionPlug() ||
		input == orientationPlug() ||
		input == scalePlug() ||
		input == attributesPlug()
	)
	{
		outputs.push_back( enginePlug() );
	}

	if(
		input == enginePlug() ||
		input == instancesPlug()->childNamesPlug()
	)
	{
		outputs.push_back( instanceChildNamesPlug() );
	}

	if(
		input == namePlug() ||
		input == instanceChildNamesPlug() ||
		input == instancesPlug()->childNamesPlug()
	)
	{
		outputs.push_back( outPlug()->childNamesPlug() );
	}

	if(
		input == enginePlug() ||
		input == namePlug() ||
		input == instancesPlug()->boundPlug() ||
		input == instancesPlug()->transformPlug() ||
		input == instanceChildNamesPlug()
	)
	{
		outputs.push_back( outPlug()->boundPlug() );
	}

	if(
		input == enginePlug() ||
		input == instancesPlug()->transformPlug()
	)
	{
		outputs.push_back( outPlug()->transformPlug() );
	}

	if( input == instancesPlug()->objectPlug() )
	{
		outputs.push_back( outPlug()->objectPlug() );
	}

	if(
		input == instancesPlug()->attributesPlug() ||
		input == enginePlug()
	)
	{
		outputs.push_back( outPlug()->attributesPlug() );
	}
}

void Instancer::hash( const Gaffer::ValuePlug *output, const Gaffer::Context *context, IECore::MurmurHash &h ) const
{
	BranchCreator::hash( output, context, h );

	if( output == enginePlug() )
	{
		inPlug()->objectPlug()->hash( h );
		positionPlug()->hash( h );
		orientationPlug()->hash( h );
		scalePlug()->hash( h );
		attributesPlug()->hash( h );
	}
	else if( output == instanceChildNamesPlug() )
	{
		enginePlug()->hash( h );
		h.append( instancesPlug()->childNamesHash( ScenePath() ) );
	}
}

void Instancer::compute( Gaffer::ValuePlug *output, const Gaffer::Context *context ) const
{
	// Both the enginePlug and instanceChildNamesPlug are evaluated
	// in a context in which scene:path holds the parent path for a
	// branch.
	if( output == enginePlug() )
	{
		static_cast<ObjectPlug *>( output )->setValue(
			new EngineData(
				inPlug()->objectPlug()->getValue(),
				indexPlug()->getValue(),
				positionPlug()->getValue(),
				orientationPlug()->getValue(),
				scalePlug()->getValue(),
				attributesPlug()->getValue()
			)
		);
		return;
	}
	else if( output == instanceChildNamesPlug() )
	{
		// Here we compute and cache the child names for all of
		// the /instances/<instanceName> locations at once. We
		// could instead compute them one at a time in
		// computeBranchChildNames() but that would require N
		// passes over the input points, where N is the number
		// of instances.
		ConstEngineDataPtr engine = boost::static_pointer_cast<const EngineData>( enginePlug()->getValue() );
		ConstInternedStringVectorDataPtr instanceNames = instancesPlug()->childNames( ScenePath() );
		vector<vector<InternedString> *> indexedInstanceChildNames;

		CompoundDataPtr result = new CompoundData;
		for( const auto &instanceName : instanceNames->readable() )
		{
			InternedStringVectorDataPtr instanceChildNames = new InternedStringVectorData;
			result->writable()[instanceName] = instanceChildNames;
			indexedInstanceChildNames.push_back( &instanceChildNames->writable() );
		}

		for( size_t i = 0, e = engine->numInstances(); i < e; ++i )
		{
			size_t index = engine->instanceIndex( i ) % indexedInstanceChildNames.size();
			indexedInstanceChildNames[index]->push_back( InternedString( i ) );
		}

		static_cast<AtomicCompoundDataPlug *>( output )->setValue( result );
		return;
	}

	BranchCreator::compute( output, context );
}

void Instancer::hashBranchBound( const ScenePath &parentPath, const ScenePath &branchPath, const Gaffer::Context *context, IECore::MurmurHash &h ) const
{
	if( branchPath.size() < 2 )
	{
		// "/" or "/instances"
		ScenePath path = parentPath;
		path.insert( path.end(), branchPath.begin(), branchPath.end() );
		if( branchPath.size() == 0 )
		{
			path.push_back( namePlug()->getValue() );
		}
		h = hashOfTransformedChildBounds( path, outPlug() );
	}
	else if( branchPath.size() == 2 )
	{
		// "/instances/<instanceName>"
		BranchCreator::hashBranchBound( parentPath, branchPath, context, h );

		engineHash( parentPath, context, h );
		instanceChildNamesHash( parentPath, context, h );
		h.append( branchPath.back() );

		{
			InstanceScope scope( context, branchPath );
			instancesPlug()->transformPlug()->hash( h );
			instancesPlug()->boundPlug()->hash( h );
		}
	}
	else
	{
		// "/instances/<instanceName>/<id>/..."
		InstanceScope instanceScope( context, branchPath );
		h = instancesPlug()->boundPlug()->hash();
	}
}

Imath::Box3f Instancer::computeBranchBound( const ScenePath &parentPath, const ScenePath &branchPath, const Gaffer::Context *context ) const
{
	if( branchPath.size() < 2 )
	{
		// "/" or "/instances"
		ScenePath path = parentPath;
		path.insert( path.end(), branchPath.begin(), branchPath.end() );
		if( branchPath.size() == 0 )
		{
			path.push_back( namePlug()->getValue() );
		}
		return unionOfTransformedChildBounds( path, outPlug() );
	}
	else if( branchPath.size() == 2 )
	{
		// "/instances/<instanceName>"
		//
		// We need to return the union of all the transformed children, but
		// because we have direct access to the engine, we can implement this
		// more efficiently than `unionOfTransformedChildBounds()`.

		ConstEngineDataPtr e = engine( parentPath, context );
		ConstCompoundDataPtr ic = instanceChildNames( parentPath, context );
		const vector<InternedString> &childNames = ic->member<InternedStringVectorData>( branchPath.back() )->readable();

		M44f childTransform;
		Box3f childBound;
		{
			InstanceScope scope( context, branchPath );
			childTransform = instancesPlug()->transformPlug()->getValue();
			childBound = instancesPlug()->boundPlug()->getValue();
		}

		typedef vector<InternedString>::const_iterator Iterator;
		typedef blocked_range<Iterator> Range;

		return parallel_reduce(
			Range( childNames.begin(), childNames.end() ),
			Box3f(),
			[ &e, &childBound, &childTransform ] ( const Range &r, Box3f u ) {
				for( Iterator i = r.begin(); i != r.end(); ++i )
				{
					const size_t index = instanceIndex( *i );
					const M44f m = childTransform * e->instanceTransform( index );
					const Box3f b = transform( childBound, m );
					u.extendBy( b );
				}
				return u;
			},
			// Union
			[] ( const Box3f &b0, const Box3f &b1 ) {
				Box3f u( b0 );
				u.extendBy( b1 );
				return u;
			}
		);
	}
	else
	{
		// "/instances/<instanceName>/<id>/..."
		InstanceScope instanceScope( context, branchPath );
		return instancesPlug()->boundPlug()->getValue();
	}
}

void Instancer::hashBranchTransform( const ScenePath &parentPath, const ScenePath &branchPath, const Gaffer::Context *context, IECore::MurmurHash &h ) const
{
	if( branchPath.size() <= 2 )
	{
		// "/" or "/instances" or "/instances/<instanceName>"
		BranchCreator::hashBranchTransform( parentPath, branchPath, context, h );
	}
	else if( branchPath.size() == 3 )
	{
		// "/instances/<instanceName>/<id>"
		BranchCreator::hashBranchTransform( parentPath, branchPath, context, h );
		{
			InstanceScope instanceScope( context, branchPath );
			instancesPlug()->transformPlug()->hash( h );
		}
		engineHash( parentPath, context, h );
		h.append( instanceIndex( branchPath ) );
	}
	else
	{
		// "/instances/<instanceName>/<id>/..."
		InstanceScope instanceScope( context, branchPath );
		h = instancesPlug()->transformPlug()->hash();
	}
}

Imath::M44f Instancer::computeBranchTransform( const ScenePath &parentPath, const ScenePath &branchPath, const Gaffer::Context *context ) const
{
	if( branchPath.size() <= 2 )
	{
		// "/" or "/instances" or "/instances/<instanceName>"
		return M44f();
	}
	else if( branchPath.size() == 3 )
	{
		// "/instances/<instanceName>/<id>"
		M44f result;
		{
			InstanceScope instanceScope( context, branchPath );
			result = instancesPlug()->transformPlug()->getValue();
		}
		ConstEngineDataPtr e = engine( parentPath, context );
		const int index = instanceIndex( branchPath );
		result = result * e->instanceTransform( index );
		return result;
	}
	else
	{
		// "/instances/<instanceName>/<id>/..."
		InstanceScope instanceScope( context, branchPath );
		return instancesPlug()->transformPlug()->getValue();
	}
}

void Instancer::hashBranchAttributes( const ScenePath &parentPath, const ScenePath &branchPath, const Gaffer::Context *context, IECore::MurmurHash &h ) const
{
	if( branchPath.size() <= 2 )
	{
		// "/" or "/instances" or "/instances/<instanceName>"
		h = outPlug()->attributesPlug()->defaultValue()->Object::hash();
	}
	else if( branchPath.size() == 3 )
	{
		// "/instances/<instanceName>/<id>"
		BranchCreator::hashBranchAttributes( parentPath, branchPath, context, h );
		{
			InstanceScope instanceScope( context, branchPath );
			instancesPlug()->attributesPlug()->hash( h );
			engineHash( parentPath, context, h );
			h.append( instanceIndex( branchPath ) );
		}
	}
	else
	{
		// "/instances/<instanceName>/<id>/...
		InstanceScope instanceScope( context, branchPath );
		h = instancesPlug()->attributesPlug()->hash();
	}
}

IECore::ConstCompoundObjectPtr Instancer::computeBranchAttributes( const ScenePath &parentPath, const ScenePath &branchPath, const Gaffer::Context *context ) const
{
	if( branchPath.size() <= 2 )
	{
		// "/" or "/instances" or "/instances/<instanceName>"
		return outPlug()->attributesPlug()->defaultValue();
	}
	else if( branchPath.size() == 3 )
	{
		// "/instances/<instanceName>/<id>"
		ConstCompoundObjectPtr baseAttributes;
		{
			InstanceScope instanceScope( context, branchPath );
			baseAttributes = instancesPlug()->attributesPlug()->getValue();
		}

		ConstEngineDataPtr e = engine( parentPath, context );
		const int index = instanceIndex( branchPath );
		CompoundObjectPtr attributes = e->instanceAttributes( index );
		if( !attributes )
		{
			return baseAttributes;
		}

		CompoundObject::ObjectMap &writableAttributes = attributes->members();
		for( auto &attribute : baseAttributes->members() )
		{
			writableAttributes.insert( attribute );
		}

		return attributes;
	}
	else
	{
		// "/instances/<instanceName>/<id>/...
		InstanceScope instanceScope( context, branchPath );
		return instancesPlug()->attributesPlug()->getValue();
	}
}

void Instancer::hashBranchObject( const ScenePath &parentPath, const ScenePath &branchPath, const Gaffer::Context *context, IECore::MurmurHash &h ) const
{
	if( branchPath.size() <= 2 )
	{
		// "/" or "/instances" or "/instances/<instanceName>"
		h = outPlug()->objectPlug()->defaultValue()->Object::hash();
	}
	else
	{
		// "/instances/<instanceName>/<id>/...
		InstanceScope instanceScope( context, branchPath );
		h = instancesPlug()->objectPlug()->hash();
	}
}

IECore::ConstObjectPtr Instancer::computeBranchObject( const ScenePath &parentPath, const ScenePath &branchPath, const Gaffer::Context *context ) const
{
	if( branchPath.size() <= 2 )
	{
		// "/" or "/instances" or "/instances/<instanceName>"
		return outPlug()->objectPlug()->defaultValue();
	}
	else
	{
		// "/instances/<instanceName>/<id>/...
		InstanceScope instanceScope( context, branchPath );
		return instancesPlug()->objectPlug()->getValue();
	}
}

void Instancer::hashBranchChildNames( const ScenePath &parentPath, const ScenePath &branchPath, const Gaffer::Context *context, IECore::MurmurHash &h ) const
{
	if( branchPath.size() == 0 )
	{
		// "/"
		BranchCreator::hashBranchChildNames( parentPath, branchPath, context, h );
		namePlug()->hash( h );
	}
	else if( branchPath.size() == 1 )
	{
		// "/instances"
		h = instancesPlug()->childNamesHash( ScenePath() );
	}
	else if( branchPath.size() == 2 )
	{
		// "/instances/<instanceName>"
		BranchCreator::hashBranchChildNames( parentPath, branchPath, context, h );
		instanceChildNamesHash( parentPath, context, h );
		h.append( branchPath.back() );
	}
	else
	{
		// "/instances/<instanceName>/<id>/..."
		InstanceScope instanceScope( context, branchPath );
		h = instancesPlug()->childNamesPlug()->hash();
	}
}

IECore::ConstInternedStringVectorDataPtr Instancer::computeBranchChildNames( const ScenePath &parentPath, const ScenePath &branchPath, const Gaffer::Context *context ) const
{
	if( branchPath.size() == 0 )
	{
		// "/"
		std::string name = namePlug()->getValue();
		if( name.empty() )
		{
			return outPlug()->childNamesPlug()->defaultValue();
		}
		InternedStringVectorDataPtr result = new InternedStringVectorData();
		result->writable().push_back( name );
		return result;
	}
	else if( branchPath.size() == 1 )
	{
		// "/instances"
		return instancesPlug()->childNames( ScenePath() );
	}
	else if( branchPath.size() == 2 )
	{
		// "/instances/<instanceName>"
		IECore::ConstCompoundDataPtr ic = instanceChildNames( parentPath, context );
		return ic->member<InternedStringVectorData>( branchPath.back() );
	}
	else
	{
		// "/instances/<instanceName>/<id>/..."
		InstanceScope instanceScope( context, branchPath );
		return instancesPlug()->childNamesPlug()->getValue();
	}
}

void Instancer::hashBranchSetNames( const ScenePath &parentPath, const Gaffer::Context *context, IECore::MurmurHash &h ) const
{
	h = instancesPlug()->setNamesPlug()->hash();
}

IECore::ConstInternedStringVectorDataPtr Instancer::computeBranchSetNames( const ScenePath &parentPath, const Gaffer::Context *context ) const
{
	return instancesPlug()->setNamesPlug()->getValue();
}

void Instancer::hashBranchSet( const ScenePath &parentPath, const IECore::InternedString &setName, const Gaffer::Context *context, IECore::MurmurHash &h ) const
{
	BranchCreator::hashBranchSet( parentPath, setName, context, h );

	h.append( instancesPlug()->childNamesHash( ScenePath() ) );
	instanceChildNamesHash( parentPath, context, h );
	instancesPlug()->setPlug()->hash( h );
	namePlug()->hash( h );
}

IECore::ConstPathMatcherDataPtr Instancer::computeBranchSet( const ScenePath &parentPath, const IECore::InternedString &setName, const Gaffer::Context *context ) const
{
	ConstInternedStringVectorDataPtr instanceNames = instancesPlug()->childNames( ScenePath() );
	IECore::ConstCompoundDataPtr instanceChildNames = this->instanceChildNames( parentPath, context );
	ConstPathMatcherDataPtr inputSet = instancesPlug()->setPlug()->getValue();

	PathMatcherDataPtr outputSetData = new PathMatcherData;
	PathMatcher &outputSet = outputSetData->writable();

	vector<InternedString> branchPath( { namePlug()->getValue() } );
	vector<InternedString> instancePath( 1 );

	for( const auto &instanceName : instanceNames->readable() )
	{
		branchPath.resize( 2 );
		branchPath.back() = instanceName;
		instancePath.back() = instanceName;

		PathMatcher instanceSet = inputSet->readable().subTree( instancePath );

		const vector<InternedString> &childNames = instanceChildNames->member<InternedStringVectorData>( instanceName )->readable();

		branchPath.push_back( InternedString() );
		for( const auto &instanceChildName : childNames )
		{
			branchPath.back() = instanceChildName;
			outputSet.addPaths( instanceSet, branchPath );
		}
	}

	return outputSetData;
}

Instancer::ConstEngineDataPtr Instancer::engine( const ScenePath &parentPath, const Gaffer::Context *context ) const
{
	ScenePlug::PathScope scope( context, parentPath );
	return boost::static_pointer_cast<const EngineData>( enginePlug()->getValue() );
}

void Instancer::engineHash( const ScenePath &parentPath, const Gaffer::Context *context, IECore::MurmurHash &h ) const
{
	ScenePlug::PathScope scope( context, parentPath );
	enginePlug()->hash( h );
}

IECore::ConstCompoundDataPtr Instancer::instanceChildNames( const ScenePath &parentPath, const Gaffer::Context *context ) const
{
	ScenePlug::PathScope scope( context, parentPath );
	return instanceChildNamesPlug()->getValue();
}

void Instancer::instanceChildNamesHash( const ScenePath &parentPath, const Gaffer::Context *context, IECore::MurmurHash &h ) const
{
	ScenePlug::PathScope scope( context, parentPath );
	instanceChildNamesPlug()->hash( h );
}

int Instancer::instanceIndex( const IECore::InternedString &name )
{
	return boost::lexical_cast<int>( name );
}

int Instancer::instanceIndex( const ScenePath &branchPath )
{
	return instanceIndex( branchPath[2] );
}

Instancer::InstanceScope::InstanceScope( const Gaffer::Context *context, const ScenePath &branchPath )
	:	EditableScope( context )
{
	assert( branchPath.size() >= 3 );
	ScenePath instancePath; instancePath.reserve( branchPath.size() - 2 );
	instancePath.push_back( branchPath[1] );
	instancePath.insert( instancePath.end(), branchPath.begin() + 3, branchPath.end() );
	set( ScenePlug::scenePathContextName, instancePath );
}
