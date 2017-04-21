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

#include "Gaffer/ContextProcessor.h"
#include "Gaffer/Context.h"
#include "Gaffer/ValuePlug.h"

namespace Gaffer
{

template<typename BaseType>
const IECore::RunTimeTyped::TypeDescription<ContextProcessor<BaseType> > ContextProcessor<BaseType>::g_typeDescription;

template<typename BaseType>
size_t ContextProcessor<BaseType>::g_firstPlugIndex = 0;

template<typename BaseType>
ContextProcessor<BaseType>::ContextProcessor( const std::string &name )
	:	BaseType( name )
{
	BaseType::storeIndexOfNextChild( g_firstPlugIndex );

	if( !BaseType::enabledPlug() )
	{
		// if the base class doesn't provide an enabledPlug(),
		// then we'll provide our own.
		BaseType::addChild( new BoolPlug( "enabled", Gaffer::Plug::In, true ) );
	}
}

template<typename BaseType>
ContextProcessor<BaseType>::~ContextProcessor()
{
}

template<typename BaseType>
BoolPlug *ContextProcessor<BaseType>::enabledPlug()
{
	if( BoolPlug *p = BaseType::enabledPlug() )
	{
		return p;
	}
	return BaseType::template getChild<BoolPlug>( g_firstPlugIndex );
}

template<typename BaseType>
const BoolPlug *ContextProcessor<BaseType>::enabledPlug() const
{
	if( const BoolPlug *p = BaseType::enabledPlug() )
	{
		return p;
	}
	return BaseType::template getChild<BoolPlug>( g_firstPlugIndex );
}

template<typename BaseType>
Plug *ContextProcessor<BaseType>::correspondingInput( const Plug *output )
{
	if( const ValuePlug *v = IECore::runTimeCast<const ValuePlug>( output ) )
	{
		return const_cast<Plug *>( static_cast<const Plug *>( oppositePlug( v ) ) );
	}
	return NULL;
}

template<typename BaseType>
const Plug *ContextProcessor<BaseType>::correspondingInput( const Plug *output ) const
{
	if( const ValuePlug *v = IECore::runTimeCast<const ValuePlug>( output ) )
	{
		return oppositePlug( v );
	}
	return NULL;
}

template<typename BaseType>
void ContextProcessor<BaseType>::affects( const Plug *input, DependencyNode::AffectedPlugsContainer &outputs ) const
{
	BaseType::affects( input, outputs );

	if( input->direction() == Plug::In )
	{
		if( const ValuePlug *inputValuePlug = IECore::runTimeCast<const ValuePlug>( input ) )
		{
			const ValuePlug *output = oppositePlug( inputValuePlug );
			if( output )
			{
				outputs.push_back( output );
			}
		}
	}
}

template<typename BaseType>
void ContextProcessor<BaseType>::appendAffectedPlugs( DependencyNode::AffectedPlugsContainer &outputs ) const
{
	const static IECore::InternedString outName( "out" );
	const Plug *outPlug = BaseType::template getChild<Plug>( outName );
	if( !outPlug )
	{
		return;
	}

	if( outPlug->children().size() )
	{
		for( RecursiveValuePlugIterator cIt( outPlug ); !cIt.done(); ++cIt )
		{
			if( outPlug->children().empty() )
			{
				outputs.push_back( cIt->get() );
			}
		}
	}
	else if( const ValuePlug *v = IECore::runTimeCast<const ValuePlug>( outPlug ) )
	{
		outputs.push_back( v );
	}
}

template<typename BaseType>
void ContextProcessor<BaseType>::hash( const ValuePlug *output, const Context *context, IECore::MurmurHash &h ) const
{
	const ValuePlug *input = oppositePlug( output );
	if( input )
	{
		if( enabledPlug()->getValue() )
		{
			ContextPtr modifiedContext = new Context( *context, Context::Borrowed );
			processContext( modifiedContext.get() );
			Context::Scope scopedContext( modifiedContext.get() );
			h = input->hash();
		}
		else
		{
			h = input->hash();
		}
		return;
	}

	BaseType::hash( output, context, h );
}

template<typename BaseType>
void ContextProcessor<BaseType>::compute( ValuePlug *output, const Context *context ) const
{
	const ValuePlug *input = oppositePlug( output );
	if( input )
	{
		if( enabledPlug()->getValue() )
		{
			ContextPtr modifiedContext = new Context( *context, Context::Borrowed );
			processContext( modifiedContext.get() );
			Context::Scope scopedContext( modifiedContext.get() );
			output->setFrom( input );
		}
		else
		{
			output->setFrom( input );
		}
		return;
	}

	return BaseType::compute( output, context );
}

template<typename BaseType>
const GraphComponent *ContextProcessor<BaseType>::correspondingDescendant( const GraphComponent *descendant, const GraphComponent *ancestor, const GraphComponent *oppositeAncestor )
{
	// this method recursively computes oppositeAncestor->descendant( plug->relativeName( plugAncestor ) ).
	// ie it finds the relative path from plugAncestor to plug, and follows it from oppositeAncestor.

	if( descendant == ancestor )
	{
		// we're already at plugAncestor, so the relative path has zero length
		// and we can return oppositeAncestor:
		return oppositeAncestor;
	}

	// now we find the corresponding descendant of plug->parent(), and
	// return its child with the same name as "plug" (if either of those things exist):

	// get parent of this plug:
	const GraphComponent *parent = descendant->parent<GraphComponent>();
	if( !parent )
	{
		// looks like the "desdendant" we initially called this function with wasn't
		// a descendant of ancestor and we've recursed up into nothing, so
		// we return NULL:
		return NULL;
	}

	// find the corresponding plug for the parent:
	const GraphComponent *oppositeParent = correspondingDescendant( parent, ancestor, oppositeAncestor );
	if( !oppositeParent )
	{
		return NULL;
	}

	// find the child corresponding to "plug"
	return oppositeParent->getChild<GraphComponent>( descendant->getName() );
}

template<typename BaseType>
const ValuePlug *ContextProcessor<BaseType>::oppositePlug( const ValuePlug *plug ) const
{
	const static IECore::InternedString inName( "in" );
	const static IECore::InternedString outName( "out" );

	const Plug *inPlug = BaseType::template getChild<Plug>( inName );
	const Plug *outPlug = BaseType::template getChild<Plug>( outName );

	if( !( outPlug && inPlug ) )
	{
		return 0;
	}

	if( plug->direction() == Plug::Out )
	{
		return IECore::runTimeCast<const ValuePlug>( correspondingDescendant( plug, outPlug, inPlug ) );
	}
	else
	{
		return IECore::runTimeCast<const ValuePlug>( correspondingDescendant( plug, inPlug, outPlug ) );
	}
}

} // namespace Gaffer
