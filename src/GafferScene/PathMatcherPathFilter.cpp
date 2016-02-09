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

#include "Gaffer/Path.h"

#include "GafferScene/PathMatcherPathFilter.h"

using namespace GafferScene;

PathMatcherPathFilter::PathMatcherPathFilter( const PathMatchers &pathMatchers, IECore::CompoundDataPtr userData )
	:	PathFilter( userData ), m_pathMatchers( pathMatchers )
{
}

PathMatcherPathFilter::~PathMatcherPathFilter()
{
}

void PathMatcherPathFilter::setPathMatchers( const PathMatchers &pathMatchers )
{
	m_pathMatchers = pathMatchers;
	changedSignal()( this );
}

const PathMatcherPathFilter::PathMatchers &PathMatcherPathFilter::getPathMatchers() const
{
	return m_pathMatchers;
}

struct PathMatcherPathFilter::Remove
{

	Remove( const PathMatchers &pathMatchers )
		:	m_pathMatchers( pathMatchers )
	{
	}

	bool operator () ( const Gaffer::PathPtr &path )
	{
		for( PathMatchers::const_iterator it = m_pathMatchers.begin(), eIt = m_pathMatchers.end(); it != eIt; ++it )
		{
			if( it->match( path->names() ) )
			{
				return false;
			}
		}
		return true;
	}

	private :

		const PathMatchers &m_pathMatchers;

};

void PathMatcherPathFilter::doFilter( std::vector<Gaffer::PathPtr> &paths ) const
{
	paths.erase(
		std::remove_if(
			paths.begin(),
			paths.end(),
			Remove( m_pathMatchers )
		),
		paths.end()
	);
}
