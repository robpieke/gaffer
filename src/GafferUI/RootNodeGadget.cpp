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

#include "IECoreGL/Selector.h"

#include "GafferUI/RootNodeGadget.h"
#include "GafferUI/SpacerGadget.h"
#include "GafferUI/GraphGadget.h"
#include "GafferUI/Style.h"

using namespace Imath;
using namespace IECore;
using namespace GafferUI;

IE_CORE_DEFINERUNTIMETYPED( RootNodeGadget );

RootNodeGadget::RootNodeGadget( Gaffer::NodePtr node )
	:	StandardNodeGadget( node )
{
	setContents( new SpacerGadget( Box3f( V3f( 0 ), V3f( 100 ) ) ) );
}

void RootNodeGadget::doRender( const Style *style ) const
{
	static Edge edges[] = { TopEdge, BottomEdge, LeftEdge, RightEdge, InvalidEdge };
	for( Edge *edge = edges; *edge != InvalidEdge; ++edge )
	{
		const Box3f bound3 = noduleContainer( *edge )->transformedBound( this );
		const Box2f bound2( V2f( bound3.min.x, bound3.min.y ), V2f( bound3.max.x, bound3.max.y ) );
		style->renderFrame( bound2, 0.5 );
	}

	NodeGadget::doRender( style );
}

void RootNodeGadget::parentChanging( Gaffer::GraphComponent *newParent )
{
	GraphGadget *graphGadget = runTimeCast<GraphGadget>( newParent );
	if( graphGadget )
	{
		m_parentRenderRequestConnection = graphGadget->renderRequestSignal().connect(
			boost::bind( &RootNodeGadget::parentRenderRequest, this, ::_1 )
		);
	}
	else
	{
		m_parentRenderRequestConnection.disconnect();
	}
}

void RootNodeGadget::parentRenderRequest( Gaffer::GraphComponent *parent )
{

	// a parent render request may mean that a node has been moved.
	// transform ourselves so that we bound all nodes.

	Box3f b;
	for( NodeGadgetIterator it( parent ); it != it.end(); ++it )
	{
		if( *it == this )
		{
			continue;
		}
		b.extendBy( (*it)->transformedBound() );
	}

	if( b.isEmpty() )
	{
		b.extendBy( V3f( 0 ) );
	}

	const V3f padding( 10, 10, 0 );
	b.min -= padding;
	b.max += padding;

	SpacerGadget *spacer = static_cast<SpacerGadget *>( getContents() );
	spacer->setSize( b );

	M44f transform;
	transform.translate( b.center() );
	setTransform( transform );

}
