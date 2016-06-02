//////////////////////////////////////////////////////////////////////////
//
//  Copyright (c) 2016, Image Engine Design Inc. All rights reserved.
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

#include "boost/bind.hpp"

#include "GafferScene/Private/IECoreScenePreview/QueueingRenderer.h"

using namespace std;
using namespace IECoreScenePreview;

//////////////////////////////////////////////////////////////////////////
// Queue. This manages a queue of arbitrary function calls and a thread
// which pops and executes them in order. The QueueingRenderer,
// QueuingAttributes and QueuingObject classes share this queue, using
// it to perform all operations on the underlying renderer.
//////////////////////////////////////////////////////////////////////////

class QueueingRenderer::Queue : public IECore::RefCounted
{

	public :

		Queue()
			:	m_thread( boost::bind( &QueueingRenderer::Queue::consumerThread, this ) )
		{
			std::cerr << "QUEUE::QUEUE" << std::endl;
			/// \todo DO WE NEED TO LIMIT THE SIZE OF THE QUEUE?
		}

		typedef boost::function<void ()> Function;

		void push( const Function &f )
		{
			m_queue.push( f );
		}

	protected :

		virtual ~Queue()
		{
			/// \todo CANCEL AND WAIT ON THREAD?
		}

	private :

		void consumerThread()
		{
			while( 1 )
			{
				Function f;
				m_queue.pop( f );
				f();
			}
		}

		tbb::concurrent_bounded_queue<Function> m_queue;
		std::thread m_thread;

};

//////////////////////////////////////////////////////////////////////////
// QueuedAttributes
//////////////////////////////////////////////////////////////////////////

class QueueingRenderer::QueuedAttributes : public IECoreScenePreview::Renderer::AttributesInterface
{

	public :

		QueuedAttributes( QueuePtr queue )
			:	m_queue( queue )
		{
		}

		IECoreScenePreview::Renderer::AttributesInterfacePtr attributes;

	protected :

		virtual ~QueuedAttributes()
		{
			/// \todo Destroy on the consumer thread!
		}

	private :

		QueuePtr m_queue;

};

//////////////////////////////////////////////////////////////////////////
// QueuedObject
//////////////////////////////////////////////////////////////////////////

class QueueingRenderer::QueuedObject : public IECoreScenePreview::Renderer::ObjectInterface
{

	public :

		QueuedObject( QueuePtr queue )
			:	m_queue( queue )
		{
		}

		ObjectInterfacePtr object;

		virtual void transform( const Imath::M44f &transform )
		{
			m_queue->push( boost::bind( &transformInternal, QueuedObjectPtr( this ), transform ) );
		}

		virtual void transform( const std::vector<Imath::M44f> &samples, const std::vector<float> &times )
		{
			m_queue->push( boost::bind( &transformInternal, QueuedObjectPtr( this ), samples, times ) );
		}

		virtual void attributes( const IECoreScenePreview::Renderer::AttributesInterface *attributes )
		{
			ConstQueuedAttributesPtr queuedAttributes = static_cast<const QueuedAttributes *>( attributes );
			m_queue->push( boost::bind( &attributesInternal, QueuedObjectPtr( this ), queuedAttributes ) );
		}

	protected :

		virtual ~QueuedObject()
		{
			/// \todo Should we destroy on the consumer thread?
		}

	private :

		static void transformInternal( QueuedObjectPtr queuedObject, const Imath::M44f &transform )
		{
			queuedObject->object->transform( transform );
		}

		static void transformInternal( QueuedObjectPtr queuedObject,  const std::vector<Imath::M44f> &samples, const std::vector<float> &times )
		{
			queuedObject->object->transform( samples, times );
		}

		static void attributesInternal( QueuedObjectPtr queuedObject, ConstQueuedAttributesPtr queuedAttributes )
		{
			//std::cerr << "attributesInternal " << object << " " << attributes << std::endl;
			queuedObject->object->attributes( queuedAttributes->attributes.get() );
		}

		QueuePtr m_queue;

};

//////////////////////////////////////////////////////////////////////////
// QueueingRenderer
//////////////////////////////////////////////////////////////////////////

QueueingRenderer::QueueingRenderer( RendererPtr renderer )
	:	m_renderer( renderer ), m_queue( new Queue() )
{
}

QueueingRenderer::~QueueingRenderer()
{
	/// SEND MESSAGE TO DISPOSE OF RENDERER ON QUEUE
	//m_thread.join();
}

void QueueingRenderer::option( const IECore::InternedString &name, const IECore::Data *value )
{
	m_queue->push( boost::bind( &optionInternal, m_renderer, name, IECore::ConstDataPtr( value ) ) );
}

void QueueingRenderer::optionInternal( RendererPtr renderer, const IECore::InternedString &name, IECore::ConstDataPtr value )
{
	renderer->option( name, value.get() );
}

void QueueingRenderer::output( const IECore::InternedString &name, const Output *output )
{
	m_queue->push( boost::bind( &outputInternal, m_renderer, name, IECore::ConstDisplayPtr( output ) ) );
}

void QueueingRenderer::outputInternal( RendererPtr renderer, const IECore::InternedString &name, IECore::ConstDisplayPtr output )
{
	renderer->output( name, output.get() );
}

Renderer::AttributesInterfacePtr QueueingRenderer::attributes( const IECore::CompoundObject *attributes )
{
	QueuedAttributesPtr result = new QueuedAttributes( m_queue );
	m_queue->push( boost::bind( &QueueingRenderer::attributesInternal, m_renderer, IECore::ConstCompoundObjectPtr( attributes ), result ) );
	return result;
}

void QueueingRenderer::attributesInternal( RendererPtr renderer, IECore::ConstCompoundObjectPtr attributes, QueuedAttributesPtr queuedAttributes )
{
	queuedAttributes->attributes = renderer->attributes( attributes.get() );
}

Renderer::ObjectInterfacePtr QueueingRenderer::camera( const std::string &name, const IECore::Camera *camera )
{
	QueuedObjectPtr result = new QueuedObject( m_queue );
	m_queue->push( boost::bind( &cameraInternal, m_renderer, name, IECore::ConstCameraPtr( camera ), result ) );
	return result;
}

void QueueingRenderer::cameraInternal( RendererPtr renderer, const std::string &name, IECore::ConstCameraPtr camera, QueuedObjectPtr queuedObject )
{
	queuedObject->object = renderer->camera( name, camera.get() );
}

Renderer::ObjectInterfacePtr QueueingRenderer::light( const std::string &name, const IECore::Object *object )
{
	QueuedObjectPtr result = new QueuedObject( m_queue );
	m_queue->push( boost::bind( &lightInternal, m_renderer, name, IECore::ConstObjectPtr( object ), result ) );
	return result;
}

void QueueingRenderer::lightInternal( RendererPtr renderer, const std::string &name, IECore::ConstObjectPtr object, QueuedObjectPtr queuedObject )
{
	queuedObject->object = renderer->light( name, object.get() );
}

Renderer::ObjectInterfacePtr QueueingRenderer::object( const std::string &name, const IECore::Object *object )
{
	QueuedObjectPtr result = new QueuedObject( m_queue );
	m_queue->push( boost::bind( &objectInternal, m_renderer, name, IECore::ConstObjectPtr( object ), result ) );
	return result;
}

void QueueingRenderer::objectInternal( RendererPtr renderer, const std::string &name, IECore::ConstObjectPtr object, QueuedObjectPtr queuedObject )
{
	queuedObject->object = renderer->object( name, object.get() );
}

Renderer::ObjectInterfacePtr QueueingRenderer::object( const std::string &name, const std::vector<const IECore::Object *> &samples, const std::vector<float> &times )
{
	QueuedObjectPtr result = new QueuedObject( m_queue );
	m_queue->push( boost::bind( &objectInternal, m_renderer, name, IECore::ConstObjectPtr( object ), result ) );
	return result;
}

/*
void objectFunction2( RendererPtr renderer, const std::string &name, const std::vector<IECore::ConstObjectPtr> samples, const std::vector<float> times, QueuedObjectPtr queuedObject )
{
	std::vector<const IECore::Object *> rawSamples; rawSamples.reserve( samples.size() );
	for( std::vector<IECore::ConstObjectPtr>::const_iterator it = samples.begin(), eIt = samples.end(); it != eIt; ++it )
	{
		rawSamples.push_back( it->get() );
	}

	queuedObject->object = renderer->object( name, rawSamples, times );
}*/

void QueueingRenderer::render()
{
	m_queue->push( boost::bind( &renderInternal, m_renderer ) );
	/// \todo WAIT FOR QUEUE TO BE EMPTY
}

void QueueingRenderer::renderInternal( RendererPtr renderer )
{
	renderer->render();
}

void QueueingRenderer::pause()
{
	m_queue->push( boost::bind( &pauseInternal, m_renderer ) );
}

void QueueingRenderer::pauseInternal( RendererPtr renderer )
{
	renderer->pause();
}

