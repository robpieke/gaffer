##########################################################################
#
#  Copyright (c) 2018, John Haddon. All rights reserved.
#
#  Redistribution and use in source and binary forms, with or without
#  modification, are permitted provided that the following conditions are
#  met:
#
#      * Redistributions of source code must retain the above
#        copyright notice, this list of conditions and the following
#        disclaimer.
#
#      * Redistributions in binary form must reproduce the above
#        copyright notice, this list of conditions and the following
#        disclaimer in the documentation and/or other materials provided with
#        the distribution.
#
#      * Neither the name of John Haddon nor the names of
#        any other contributors to this software may be used to endorse or
#        promote products derived from this software without specific prior
#        written permission.
#
#  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
#  IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
#  THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
#  PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
#  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
#  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
#  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
#  PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
#  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
#  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
#  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
##########################################################################

import os
import unittest

import Gaffer
import GafferTest
import GafferScene
import GafferSceneTest
import GafferRenderMan

@unittest.skipIf( "TRAVIS" in os.environ, "No license available on Travis" )
class InteractiveRenderManRenderTest( GafferSceneTest.InteractiveRenderTest ) :

	def _createInteractiveRender( self ) :

		return GafferRenderMan.InteractiveRenderManRender()

	def _createConstantShader( self ) :

		shader = GafferRenderMan.RenderManShader()
		shader.loadShader( "PxrConstant" )
		return shader, shader["parameters"]["emitColor"]

	def _createTraceSetShader( self ) :

		return None, None

	def _cameraVisibilityAttribute( self ) :

		return "dl:visibility.camera"

	def _createMatteShader( self ) :

		shader = GafferOSL.OSLShader()
		shader.loadShader( "matte" )
		return shader, shader["parameters"]["Cs"]

	def _createPointLight( self ) :

		light = GafferOSL.OSLLight()
		light["shape"].setValue( light.Shape.Sphere )
		light["radius"].setValue( 0.01 )
		light.loadShader( "maya/osl/pointLight" )
		light["attributes"].addMember( "dl:visibility.camera", False )

		return light, light["parameters"]["i_color"]

if __name__ == "__main__":
	unittest.main()
