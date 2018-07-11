//////////////////////////////////////////////////////////////////////////
//
//  Copyright (c) 2018, John Haddon. All rights reserved.
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

#include "SceneGraphAlgo.h"

#include "GafferScene/Private/IECoreScenePreview/Renderer.h"

#include "IECoreScene/Shader.h"

#include "IECore/DataAlgo.h"
#include "IECore/MessageHandler.h"
#include "IECore/ObjectVector.h"
// #include "IECore/SearchPath.h"
#include "IECore/SimpleTypedData.h"
// #include "IECore/StringAlgo.h"

#include "boost/algorithm/string.hpp"
#include "boost/algorithm/string/predicate.hpp"

#include "RixSceneGraph.h"
#include "RixPredefinedStrings.hpp"

//#include "tbb/concurrent_hash_map.h"

#include <unordered_map>

using namespace std;
using namespace Imath;
using namespace IECore;
using namespace IECoreScene;
using namespace IECoreRenderMan;

//////////////////////////////////////////////////////////////////////////
// Utilities
//////////////////////////////////////////////////////////////////////////

namespace
{

template<typename T>
T *reportedCast( const IECore::RunTimeTyped *v, const char *type, const IECore::InternedString &name )
{
	T *t = IECore::runTimeCast<T>( v );
	if( t )
	{
		return t;
	}

	IECore::msg( IECore::Msg::Warning, "IECoreDelight::Renderer", boost::format( "Expected %s but got %s for %s \"%s\"." ) % T::staticTypeName() % v->typeName() % type % name.c_str() );
	return nullptr;
}

template<typename T, typename MapType>
const T *parameter( const MapType &parameters, const IECore::InternedString &name )
{
	auto it = parameters.find( name );
	if( it == parameters.end() )
	{
		return nullptr;
	}

	return reportedCast<const T>( it->second.get(), "parameter", name );
}

template<typename T>
T parameter( const IECore::CompoundDataMap &parameters, const IECore::InternedString &name, const T &defaultValue )
{
	typedef IECore::TypedData<T> DataType;
	if( const DataType *d = parameter<DataType>( parameters, name ) )
	{
		return d->readable();
	}
	else
	{
		return defaultValue;
	}
}

} // namespace

//////////////////////////////////////////////////////////////////////////
// Shaders
//////////////////////////////////////////////////////////////////////////

namespace
{

InternedString g_handle( "__handle" );
InternedString g_surfaceShaderAttributeName( "renderman:bxdf" );

struct ParameterEmitter
{

	void operator()( const IntData *data, const InternedString &name, RixParamList *paramList ) const
	{
		paramList->SetInteger( RtUString( name.c_str() ), data->readable() );
	}

	void operator()( const FloatData *data, const InternedString &name, RixParamList *paramList ) const
	{
		paramList->SetFloat( RtUString( name.c_str() ), data->readable() );
	}

	void operator()( const StringData *data, const InternedString &name, RixParamList *paramList ) const
	{
		paramList->SetString( RtUString( name.c_str() ), RtUString( data->readable().c_str() ) );
	}

	void operator()( const Color3fData *data, const InternedString &name, RixParamList *paramList ) const
	{
		paramList->SetColor( RtUString( name.c_str() ), RtColorRGB( data->readable().getValue() ) );
	}

	void operator()( const IntVectorData *data, const InternedString &name, RixParamList *paramList ) const
	{
		paramList->SetIntegerArray( RtUString( name.c_str() ), data->readable().data(), data->readable().size() );
	}

	void operator()( const Data *data, const InternedString &name, RixParamList *paramList ) const
	{
		IECore::msg(
			IECore::Msg::Warning,
			"IECoreRenderMan",
			boost::format( "Unsupported parameter \"%s\" of type \"%s\"" ) % name % data->typeName()
		);
	}

};

vector<RixSGShader *> convertShader( const ObjectVector *network, RixSGScene *scene )
{
	vector<RixSGShader *> result;
	result.reserve( network->members().size() );
	for( auto &object : network->members() )
	{
		const Shader *shader = runTimeCast<const Shader>( object.get() );
		if( !shader )
		{
			continue;
		}

		RixShadingInterface type = k_RixInvalid;
		if( shader->getType() == "renderman:bxdf" )
		{
			type = k_RixBxdfFactory;
		}
		else if( shader->getType() == "renderman:shader" )
		{
			type = k_RixPattern;
		}

		string handle = parameter<string>( shader->parameters(), g_handle, "" );
		RixSGShader *sgShader = scene->CreateShader( type, RtUString( shader->getName().c_str() ), RtUString( handle.c_str() ) );

		RixParamList *paramList = sgShader->EditParameterBegin();

		for( const auto &parameter : shader->parameters() )
		{
			dispatch( parameter.second.get(), ParameterEmitter(), parameter.first, paramList );
		}

		sgShader->EditParameterEnd( paramList );
		result.push_back( sgShader );
	}

	return result;
}

vector<RixSGShader *> defaultShader( RixSGScene *scene )
{
	vector<RixSGShader *> result;

	result.push_back( scene->CreateShader( k_RixPattern, RtUString( "PxrFacingRatio" ), RtUString( "facingRatio" ) ) );

	RixSGShader *toFloat3 = scene->CreateShader( k_RixPattern, RtUString( "PxrToFloat3" ), RtUString( "toFloat3" ) );
	result.push_back( toFloat3 );

	RixParamList *paramList = toFloat3->EditParameterBegin();
	paramList->ReferenceFloat( RtUString( "input" ), RtUString( "facingRatio:resultF" ) );
	toFloat3->EditParameterEnd( paramList );

	RixSGShader *constant = scene->CreateShader( k_RixBxdfFactory, RtUString( "PxrConstant" ), RtUString( "constant" ) );
	result.push_back( constant );

   	paramList = constant->EditParameterBegin();
   	paramList->ReferenceColor( RtUString( "emitColor" ), RtUString( "toFloat3:resultRGB" ) );
    constant->EditParameterEnd( paramList );

	return result;
}

} // namespace

//////////////////////////////////////////////////////////////////////////
// RenderManAttributes
//////////////////////////////////////////////////////////////////////////

namespace
{

class RenderManAttributes : public IECoreScenePreview::Renderer::AttributesInterface
{

	public :

		RenderManAttributes( RixSGScene *scene, const IECore::CompoundObject *attributes )
		{
			if( const ObjectVector *network = parameter<ObjectVector>( attributes->members(), g_surfaceShaderAttributeName ) )
			{
				m_shader = convertShader( network, scene );
			}
			else
			{
				m_shader = defaultShader( scene );
			}
			m_material = scene->CreateMaterial( RtUString( "materialIdentifier" ) ); // FIX ME!
			m_material->SetBxdf( m_shader.size(), m_shader.data() );
		}

		~RenderManAttributes()
		{
			/// \todo Dispose of material and shaders
		}

		void apply( RixSGGroup *group ) const
		{
			group->SetMaterial( m_material );
		}

	private :

		vector<RixSGShader *> m_shader;
		RixSGMaterial *m_material;

};

IE_CORE_DECLAREPTR( RenderManAttributes )

} // namespace

//////////////////////////////////////////////////////////////////////////
// RenderManObject
//////////////////////////////////////////////////////////////////////////

namespace
{

class RenderManObject : public IECoreScenePreview::Renderer::ObjectInterface
{

	public :

		RenderManObject( RixSGGroup *group, RixSGScene *deleter )
			:	m_group( group ), m_deleter( deleter )
		{
		}

		~RenderManObject()
		{
			if( m_deleter )
			{
				m_deleter->DeleteDagNode( m_group );
			}
		}

		void transform( const Imath::M44f &transform ) override
		{
			m_group->SetTransform( reinterpret_cast<const RtMatrix4x4 &>( transform ) );
		}

		void transform( const std::vector<Imath::M44f> &samples, const std::vector<float> &times ) override
		{
			m_group->SetTransform( samples.size(), reinterpret_cast<const RtMatrix4x4 *>( samples.data() ), times.data() );
		}

		bool attributes( const IECoreScenePreview::Renderer::AttributesInterface *attributes ) override
		{
			static_cast<const RenderManAttributes *>( attributes )->apply( m_group );
			return true;
		}

	private :

		RixSGGroup *m_group;
		// Null unless we're rendering interactively
		RixSGScene *m_deleter;

};

} // namespace

//////////////////////////////////////////////////////////////////////////
// RenderManRenderer
//////////////////////////////////////////////////////////////////////////

namespace
{

/// \todo Set these properly
void setOptions( RixSGScene *scene )
{
    RixParamList* options = scene->EditOptionBegin();
    int32_t const format[2] = { 512, 512 };
    options->SetIntegerArray(Rix::k_Ri_FormatResolution, format, 2);
    options->SetFloat(Rix::k_Ri_FormatPixelAspectRatio, 1.0f);
    options->SetInteger(Rix::k_hider_minsamples, 4);
    options->SetInteger(Rix::k_hider_maxsamples, 16);
    scene->EditOptionEnd(options);
}

class RenderManRenderer final : public IECoreScenePreview::Renderer
{

	public :

		RenderManRenderer( RenderType renderType, const std::string &fileName )
			:	m_renderType( renderType ),
				m_fileName( fileName ),
				m_sceneManager( (RixSGManager *)RixGetContext()->GetRixInterface( k_RixSGManager ) ),
				m_scene( m_sceneManager->CreateScene() ),
				m_displaysDirty( false ),
				m_state( Stopped )
		{
   			RixParamList *options = m_scene->EditOptionBegin();

   			options->SetString( Rix::k_searchpath_display, RtUString( "/Users/john/dev/build/gaffer/renderMan/displayDrivers" ) );

    		m_scene->EditOptionEnd( options );


		    RixSGGroup* root = m_scene->Root();

			setOptions( m_scene );

			// Set integrator
			RixSGShader *integrator = m_scene->CreateShader( k_RixIntegrator, RtUString( "PxrPathTracer" ), RtUString( "integrator" ) );
			m_scene->SetIntegrator( 1, &integrator );

			m_camera = m_scene->CreateCamera( RtUString( "eye" ) );
			m_camera->SetOrientTransform(  RtMatrix4x4( RtVector3( 1, 1, -1 ) ) );
			m_camera->SetRenderable( true );

			RixSGShader *proj = m_scene->CreateShader( k_RixProjectionFactory, RtUString( "PxrPerspective" ), RtUString( "proj" ) );
			m_camera->SetProjection( 1, &proj );

			root->AddChild( m_camera );
		}

		~RenderManRenderer() override
		{
			m_sceneManager->DeleteScene( m_scene->sceneId );
		}

		IECore::InternedString name() const override
		{
			return "RenderMan";
		}

		void option( const IECore::InternedString &name, const IECore::Object *value ) override
		{
		}

		void output( const IECore::InternedString &name, const Output *output ) override
		{
			if( output )
			{
				m_outputs[name] = output->copy();
			}
			else
			{
				m_outputs.erase( name );
			}
			m_displaysDirty = true;
		}

		Renderer::AttributesInterfacePtr attributes( const IECore::CompoundObject *attributes ) override
		{
			return new RenderManAttributes( m_scene, attributes );
		}

		ObjectInterfacePtr camera( const std::string &name, const IECoreScene::Camera *camera, const AttributesInterface *attributes ) override
		{
			return nullptr;
		}

		ObjectInterfacePtr light( const std::string &name, const IECore::Object *object, const AttributesInterface *attributes ) override
		{
			return this->object( name, object, attributes );
		}

		ObjectInterfacePtr lightFilter( const std::string &name, const IECore::Object *object, const AttributesInterface *attributes ) override
		{
			return nullptr;
		}

		Renderer::ObjectInterfacePtr object( const std::string &name, const IECore::Object *object, const AttributesInterface *attributes ) override
		{
			RixSGGroup *group = SceneGraphAlgo::convert( object, m_scene, RtUString( name.c_str() ) );
			if( !group )
			{
				return nullptr;
			}

			m_scene->Root()->AddChild( group );

			ObjectInterfacePtr result = new RenderManObject( group, m_renderType == Interactive ? m_scene : nullptr );
			result->attributes( attributes );

			return result;
		}

		ObjectInterfacePtr object( const std::string &name, const std::vector<const IECore::Object *> &samples, const std::vector<float> &times, const AttributesInterface *attributes ) override
		{
			return nullptr;
		}

		void render() override
		{
			updateDisplays();

			switch( m_renderType )
			{
   				case SceneDescription : {
   					string command = "rib " + m_fileName;
   					m_scene->Render( command.c_str() );
   					break;
   				}
   				case Batch : {
				    // WHERE TO PUT THIS?? ALSO NEED TO CHECK ARGUMENTS
				    // DOCS SUGGEST PASSING "" for argv[0]?
				    const char *argv[] = { "" };
				    PRManBegin( 1, (char **)argv );
    				m_scene->Render( "prman -blocking" );
     				break;
     			}
     			case Interactive :
				    if( m_state == Stopped )
				    {
				    	PRManBegin(0, 0);
						m_scene->Render( "prman -live" );
					}
					else if( m_state == Paused )
					{
						m_scene->EditEnd();
					}
					m_state = Running;

    				break;
			}
		}

		void pause() override
		{
			if( m_state == Running )
			{
				m_scene->EditBegin();
				m_state = Paused;
			}
		}

	private :

		void updateDisplays()
		{
			if( !m_displaysDirty )
			{
				return;
			}

			/// \todo Convert data etc into DisplayChannels,
			/// and convert additional parameters so our display
			/// driver starts to work.

			vector<RixSGDisplay> displays;
			for( const auto &output : m_outputs )
			{
				string type = output.second->getType();
				if( type == "exr" )
				{
					type = "openexr";
				}
				RixSGDisplay display = m_scene->CreateDisplay(
					RtUString( type.c_str() ),
					RtUString( output.second->getName().c_str() )
				);

				for( const auto &parameter : output.second->parameters() )
				{
					dispatch( parameter.second.get(), ParameterEmitter(), parameter.first, display.params );
				}

				displays.push_back( display );
			}

			m_camera->SetDisplay( displays.size(), displays.data() );

			m_displaysDirty = false;
		}

		RenderType m_renderType;
		const std::string m_fileName;
		RixSGManager *m_sceneManager;
		RixSGScene *m_scene;

		std::unordered_map<InternedString, ConstOutputPtr> m_outputs;
		bool m_displaysDirty;

		RixSGCamera *m_camera;

		enum State
		{
			Stopped,
			Running,
			Paused
		};

		State m_state;

		static Renderer::TypeDescription<RenderManRenderer> g_typeDescription;

};

IECoreScenePreview::Renderer::TypeDescription<RenderManRenderer> RenderManRenderer::g_typeDescription( "RenderMan" );

} // namespace
