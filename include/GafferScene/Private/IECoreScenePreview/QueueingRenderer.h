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

#ifndef IECORESCENEPREVIEW_QUEUEINGRENDERER_H
#define IECORESCENEPREVIEW_QUEUEINGRENDERER_H

#include "tbb/compat/thread"
#include "tbb/concurrent_queue.h"

#include "boost/function.hpp"

#include "GafferScene/Private/IECoreScenePreview/Renderer.h"

namespace IECoreScenePreview
{

class QueueingRenderer : public Renderer
{

	public :

		QueueingRenderer( RendererPtr renderer );

		IE_CORE_DECLAREMEMBERPTR( QueueingRenderer )

		virtual void option( const IECore::InternedString &name, const IECore::Data *value );
		virtual void output( const IECore::InternedString &name, const Output *output );

		virtual AttributesInterfacePtr attributes( const IECore::CompoundObject *attributes );

		virtual ObjectInterfacePtr camera( const std::string &name, const IECore::Camera *camera );
		virtual ObjectInterfacePtr light( const std::string &name, const IECore::Object *object = NULL );
		virtual ObjectInterfacePtr object( const std::string &name, const IECore::Object *object );
		virtual ObjectInterfacePtr object( const std::string &name, const std::vector<const IECore::Object *> &samples, const std::vector<float> &times );

		virtual void render();
		virtual void pause();

	protected :

		virtual ~QueueingRenderer();

	private :

		IE_CORE_FORWARDDECLARE( Queue );
		IE_CORE_FORWARDDECLARE( QueuedAttributes );
		IE_CORE_FORWARDDECLARE( QueuedObject );

		static void optionInternal( RendererPtr renderer, const IECore::InternedString &name, IECore::ConstDataPtr value );
		static void outputInternal( RendererPtr renderer, const IECore::InternedString &name, IECore::ConstDisplayPtr output );

		static void attributesInternal( RendererPtr renderer, IECore::ConstCompoundObjectPtr attributes, QueuedAttributesPtr queuedAttributes );

		static void cameraInternal( RendererPtr renderer, const std::string &name, IECore::ConstCameraPtr camera, QueuedObjectPtr queuedObject );
		static void lightInternal( RendererPtr renderer, const std::string &name, IECore::ConstObjectPtr object, QueuedObjectPtr queuedObject );
		static void objectInternal( RendererPtr renderer, const std::string &name, IECore::ConstObjectPtr object, QueuedObjectPtr queuedObject );
		static void objectInternal( const std::string &name, const std::vector<const IECore::Object *> &samples, const std::vector<float> &times );

		static void renderInternal( RendererPtr renderer );
		static void pauseInternal( RendererPtr renderer );

		RendererPtr m_renderer;
		QueuePtr m_queue;

};

IE_CORE_DECLAREPTR( QueueingRenderer )

} // namespace IECoreScenePreview

#endif // IECORESCENEPREVIEW_QUEUEINGRENDERER_H
