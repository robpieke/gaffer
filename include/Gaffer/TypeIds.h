//////////////////////////////////////////////////////////////////////////
//  
//  Copyright (c) 2011, John Haddon. All rights reserved.
//  Copyright (c) 2011, Image Engine Design Inc. All rights reserved.
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

#ifndef GAFFER_TYPEIDS_H
#define GAFFER_TYPEIDS_H

namespace Gaffer
{

enum TypeId
{

	GraphComponentTypeId = 110000,
	NodeTypeId = 110001,
	PlugTypeId = 110002,
	ValuePlugTypeId = 110003,
	FloatPlugTypeId = 110004,
	IntPlugTypeId = 110005,
	StringPlugTypeId = 110006,
	ScriptNodeTypeId = 110007,
	ApplicationRootTypeId = 110008,
	ScriptContainerTypeId = 110009,
	SetTypeId = 110010,
	ObjectPlugTypeId = 110011,
	CompoundPlugTypeId = 110012,
	V2fPlugTypeId = 110013,
	V3fPlugTypeId = 110014,
	V2iPlugTypeId = 110015,
	V3iPlugTypeId = 110016,
	Color3fPlugTypeId = 110017,
	Color4fPlugTypeId = 110018,
	SplineffPlugTypeId = 110019,
	SplinefColor3fPlugTypeId = 110020,
	M33fPlugTypeId = 110021,
	M44fPlugTypeId = 110022,
	BoolPlugTypeId = 110023,
	ParameterisedHolderNodeTypeId = 110024,
	IntVectorDataPlugTypeId = 110025,
	FloatVectorDataPlugTypeId = 110026,
	StringVectorDataPlugTypeId = 110027,
	V3fVectorDataPlugTypeId = 110028,
	StandardSetTypeId = 110029,
	ChildSetTypeId = 110030,
	BoolVectorDataPlugTypeId = 110031,
	
	LastTypeId = 110500,
	
};

} // namespace Gaffer

#endif // GAFFER_TYPEIDS_H
