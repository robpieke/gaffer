//////////////////////////////////////////////////////////////////////////
//
//  Copyright (c) 2015, John Haddon. All rights reserved.
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

#include "IECore/SceneInterface.h"

#include "GafferVDB/TypeIds.h"

using namespace Imath;
using namespace IECore;
using namespace GafferVDB;

namespace
{

class VDBScene : public SceneInterface
{

	public :

		IE_CORE_DECLARERUNTIMETYPEDEXTENSION( VDBScene, VDBSceneTypeId, IECore::SceneInterface );

		VDBScene( const std::string &fileName, IndexedIO::OpenMode openMode )
		{
		}

		virtual ~VDBScene()
		{
		}

		virtual std::string fileName() const
		{
			return "";
		}

		virtual Name name() const;
		virtual void path( Path &p ) const;

		////////////////////////////////////////////////////
		// Bounds
		////////////////////////////////////////////////////

		virtual Imath::Box3d readBound( double time ) const
		{
			return Box3d();
		}

		virtual void writeBound( const Imath::Box3d &bound, double time );

		////////////////////////////////////////////////////
		// Transforms
		////////////////////////////////////////////////////

		virtual ConstDataPtr readTransform( double time ) const
		{
			return NULL;
		}

		virtual Imath::M44d readTransformAsMatrix( double time ) const
		{
			return M44d();
		}

		virtual void writeTransform( const Data *transform, double time )
		{

		}

		////////////////////////////////////////////////////
		// Attributes
		////////////////////////////////////////////////////

		virtual bool hasAttribute( const Name &name ) const
		{
			return false;
		}

		/// Fills attrs with the names of all attributes available in the current directory
		virtual void attributeNames( NameList &attrs ) const
		{
		}

		virtual ConstObjectPtr readAttribute( const Name &name, double time ) const
		{
			return NULL;
		}

		virtual void writeAttribute( const Name &name, const Object *attribute, double time )
		{
		}

		////////////////////////////////////////////////////
		// Tags
		////////////////////////////////////////////////////

		virtual bool hasTag( const Name &name, int filter = LocalTag ) const
		{
			return false;
		}

		virtual void readTags( NameList &tags, int filter = LocalTag ) const
		{
		}

		virtual void writeTags( const NameList &tags )
		{
		}

		////////////////////////////////////////////////////
		// Objects
		////////////////////////////////////////////////////

		virtual bool hasObject() const
		{
			return false;
		}

		virtual ConstObjectPtr readObject( double time ) const
		{
			return NULL;
		}

		virtual PrimitiveVariableMap readObjectPrimitiveVariables( const std::vector<InternedString> &primVarNames, double time ) const
		{
			return PrimitiveVariableMap();
		}

		virtual void writeObject( const Object *object, double time )
		{
		}

		////////////////////////////////////////////////////
		// Hierarchy
		////////////////////////////////////////////////////

		/// Convenience method to determine if a child exists
		virtual bool hasChild( const Name &name ) const;
		/// Queries the names of any existing children of path() within
		/// the scene.
		virtual void childNames( NameList &childNames ) const;
		/// Returns an object for the specified child location in the scene.
		/// If the child does not exist then it will behave according to the
		/// missingBehavior parameter. May throw and exception, may return a NULL pointer,
		/// or may create the child (if that is possible).
		/// Bounding boxes will be automatically propagated up from the children
		/// to the parent as it is written.
		virtual SceneInterfacePtr child( const Name &name, MissingBehaviour missingBehaviour = ThrowIfMissing );
		/// Returns a read-only interface for a child location in the scene.
		virtual ConstSceneInterfacePtr child( const Name &name, MissingBehaviour missingBehaviour = ThrowIfMissing ) const;
		/// Returns a writable interface to a new child. Throws an exception if it already exists.
		/// Bounding boxes will be automatically propagated up from the children
		/// to the parent as it is written.
		virtual SceneInterfacePtr createChild( const Name &name );
		/// Returns a interface for querying the scene at the given path (full path).
		virtual SceneInterfacePtr scene( const Path &path, MissingBehaviour missingBehaviour = ThrowIfMissing );
		/// Returns a const interface for querying the scene at the given path (full path).
		virtual ConstSceneInterfacePtr scene( const Path &path, MissingBehaviour missingBehaviour = ThrowIfMissing ) const;

		////////////////////////////////////////////////////
		// Hash
		////////////////////////////////////////////////////

		virtual void hash( HashType hashType, double time, MurmurHash &h ) const;

	private :

		static FileFormatDescription<VDBScene> g_description;//( "vdb", IndexedIO::Read );

};

} // namespace
