//////////////////////////////////////////////////////////////////////////
//
//  Copyright (c) 2014, Image Engine Design Inc. All rights reserved.
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

#include "IECoreGL/Texture.h"

#include "Gaffer/UndoContext.h"
#include "Gaffer/ScriptNode.h"
#include "Gaffer/Box.h"

#include "GafferUI/Nodule.h"
#include "GafferUI/ImageGadget.h"
#include "GafferUI/PlugPromoter.h"
#include "GafferUI/Style.h"

using namespace Imath;
using namespace IECore;
using namespace Gaffer;
using namespace GafferUI;

IE_CORE_DEFINERUNTIMETYPED( PlugPromoter );

PlugPromoter::PlugPromoter( Gaffer::BoxPtr box )
	:	m_box( box )
{
	dragEnterSignal().connect( boost::bind( &PlugPromoter::dragEnter, this, ::_2 ) );
	dragLeaveSignal().connect( boost::bind( &PlugPromoter::dragLeave, this, ::_2 ) );
	dropSignal().connect( boost::bind( &PlugPromoter::drop, this, ::_2 ) );
}

PlugPromoter::~PlugPromoter()
{
}

Imath::Box3f PlugPromoter::bound() const
{
	return Box3f( V3f( -0.5f, -0.5f, 0.0f ), V3f( 0.5f, 0.5f, 0.0f ) );
}

/*static IECoreGL::Texture *texture()
{
	static IECoreGL::TexturePtr t = NULL;
	if( !t )
	{
		t = ImageGadget::textureLoader()->load( "plugPromoter.png" );
		IECoreGL::Texture::ScopedBinding binding( *t );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
		//glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_LOD_BIAS, -1.25 );
	}
	return t.get();
}*/

void PlugPromoter::doRender( const Style *style ) const
{
	float radius = 0.5f;
	Style::State state = Style::NormalState;
	if( getHighlighted() )
	{
		state = Style::HighlightedState;
		radius = 1.0f;
	}
	style->renderNodule( radius, state );
	//style->renderImage( Box2f( V2f( -radius ), V2f( radius ) ), texture() );
}

bool PlugPromoter::dragEnter( const DragDropEvent &event )
{
	const Plug *plug = runTimeCast<Plug>( event.data.get() );
	if( !plug )
	{
		return false;
	}

	Nodule *sourceNodule = runTimeCast<Nodule>( event.sourceGadget.get() );
	if( !sourceNodule )
	{
		return false;
	}

	const Node *node = plug->node();
	if( node->parent<Node>() == m_box )
	{
		if( !m_box->canPromotePlug( plug, /* asUserPlug = */ false ) )
		{
			return false;
		}
	}
	else
	{
		if( node->parent<Node>() != m_box->parent<Node>() )
		{
			return false;
		}
	}

	setHighlighted( true );

	V3f center = V3f( 0.0f ) * fullTransform();
	center = center * sourceNodule->fullTransform().inverse();
	sourceNodule->updateDragEndPoint( center, V3f( 0, -1, 0 ) );

	return true;
}

bool PlugPromoter::dragLeave( const DragDropEvent &event )
{
	setHighlighted( false );
	return true;
}

bool PlugPromoter::drop( const DragDropEvent &event )
{
	setHighlighted( false );

	Plug *plug = static_cast<Plug *>( event.data.get() );
	UndoContext undoEnabler( plug->ancestor<ScriptNode>() );

	if( m_box->isAncestorOf( plug ) )
	{
		m_box->promotePlug( plug, /* asUserPlug = */ false );
	}
	else
	{
		PlugPtr boxPlug = plug->createCounterpart( plug->getName(), plug->direction() == Plug::In ? Plug::Out : Plug::In );
		m_box->addChild( boxPlug );
		if( boxPlug->direction() == Plug::In )
		{
			boxPlug->setInput( plug );
		}
		else
		{
			plug->setInput( boxPlug );
		}
	}

	return true;
}
