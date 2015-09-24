##########################################################################
#
#  Copyright (c) 2013-2015, Image Engine Design Inc. All rights reserved.
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
import shutil
import collections

import IECore

import Gaffer
import GafferTest

class ReferenceTest( GafferTest.TestCase ) :

	def setUp( self ) :

		GafferTest.TestCase.setUp( self )

		# stash the SphereNode so we can restore it in
		# tearDown() - we're going to mischievously delete
		# it from the GafferTest module to induce errors
		# during testing.
		self.__SphereNode = GafferTest.SphereNode

	def testLoad( self ) :

		s = Gaffer.ScriptNode()

		s["n1"] = GafferTest.AddNode()
		s["n2"] = GafferTest.AddNode()
		s["n2"]["op1"].setInput( s["n1"]["sum"] )

		b = Gaffer.Box.create( s, Gaffer.StandardSet( [ s["n1"] ] ) )

		b.exportForReference( self.temporaryDirectory() + "/test.grf" )

		s["r"] = Gaffer.Reference()
		s["r"].load( self.temporaryDirectory() + "/test.grf" )

		self.assertTrue( "n1" in s["r"] )
		self.assertTrue( s["r"]["sum"].getInput().isSame( s["r"]["n1"]["sum"] ) )

	def testSerialisation( self ) :

		s = Gaffer.ScriptNode()

		s["n1"] = GafferTest.AddNode()
		s["n2"] = GafferTest.AddNode()
		s["n2"]["op1"].setInput( s["n1"]["sum"] )
		b = Gaffer.Box.create( s, Gaffer.StandardSet( [ s["n1"] ] ) )
		b.promotePlug( b["n1"]["op1"] )

		b.exportForReference( self.temporaryDirectory() + "/test.grf" )

		s = Gaffer.ScriptNode()
		s["r"] = Gaffer.Reference()
		s["r"].load( self.temporaryDirectory() + "/test.grf" )

		self.assertTrue( "n1" in s["r"] )
		self.assertTrue( s["r"]["n1"]["op1"].getInput().isSame( s["r"]["op1"] ) )
		self.assertTrue( s["r"]["sum"].getInput().isSame( s["r"]["n1"]["sum"] ) )

		s["r"]["op1"].setValue( 25 )
		self.assertEqual( s["r"]["sum"].getValue(), 25 )

		ss = s.serialise()

		# referenced nodes should be referenced only, and not
		# explicitly mentioned in the serialisation at all.
		self.assertTrue( "AddNode" not in ss )
		# but the values of user plugs should be stored, so
		# they can override the values from the reference.
		self.assertTrue( "\"op1\"" in ss )

		s2 = Gaffer.ScriptNode()
		s2.execute( ss )

		self.assertTrue( "n1" in s2["r"] )
		self.assertTrue( s2["r"]["sum"].getInput().isSame( s2["r"]["n1"]["sum"] ) )
		self.assertEqual( s2["r"]["sum"].getValue(), 25 )

	def testReload( self ) :

		s = Gaffer.ScriptNode()

		s["n1"] = GafferTest.AddNode()
		s["n2"] = GafferTest.AddNode()
		s["n3"] = GafferTest.AddNode()
		s["n2"]["op1"].setInput( s["n1"]["sum"] )
		s["n3"]["op1"].setInput( s["n2"]["sum"] )
		b = Gaffer.Box.create( s, Gaffer.StandardSet( [ s["n2"] ] ) )
		b.promotePlug( b["n2"]["op2"] )

		b.exportForReference( self.temporaryDirectory() + "/test.grf" )

		s2 = Gaffer.ScriptNode()
		s2["n1"] = GafferTest.AddNode()
		s2["n3"] = GafferTest.AddNode()
		s2["r"] = Gaffer.Reference()
		s2["r"].load( self.temporaryDirectory() + "/test.grf" )

		s2["r"]["op1"].setInput( s2["n1"]["sum"] )
		s2["r"]["op2"].setValue( 1001 )
		s2["n3"]["op1"].setInput( s2["r"]["sum"] )

		self.assertTrue( "n2" in s2["r"] )
		self.assertTrue( s2["r"]["n2"]["op1"].getInput().isSame( s2["r"]["op1"] ) )
		self.assertTrue( s2["r"]["n2"]["op2"].getInput().isSame( s2["r"]["op2"] ) )
		self.assertEqual( s2["r"]["op2"].getValue(), 1001 )
		self.assertTrue( s2["r"]["sum"].getInput().isSame( s2["r"]["n2"]["sum"] ) )
		self.assertTrue( s2["r"]["op1"].getInput().isSame( s2["n1"]["sum"] ) )
		self.assertTrue( s2["n3"]["op1"].getInput().isSame( s2["r"]["sum"] ) )
		originalReferencedNames = s2["r"].keys()

		b["anotherNode"] = GafferTest.AddNode()
		b.promotePlug( b["anotherNode"]["op2"] )
		s.serialiseToFile( self.temporaryDirectory() + "/test.grf", b )

		s2["r"].load( self.temporaryDirectory() + "/test.grf" )

		self.assertTrue( "n2" in s2["r"] )
		self.assertEqual( set( s2["r"].keys() ), set( originalReferencedNames + [ "anotherNode", "op3" ] ) )
		self.assertTrue( s2["r"]["n2"]["op1"].getInput().isSame( s2["r"]["op1"] ) )
		self.assertTrue( s2["r"]["n2"]["op2"].getInput().isSame( s2["r"]["op2"] ) )
		self.assertEqual( s2["r"]["op2"].getValue(), 1001 )
		self.assertTrue( s2["r"]["anotherNode"]["op2"].getInput().isSame( s2["r"]["op3"] ) )
		self.assertTrue( s2["r"]["sum"].getInput().isSame( s2["r"]["n2"]["sum"] ) )
		self.assertTrue( s2["r"]["op1"].getInput().isSame( s2["n1"]["sum"] ) )
		self.assertTrue( s2["n3"]["op1"].getInput().isSame( s2["r"]["sum"] ) )

	def testReloadDoesntRemoveCustomPlugs( self ) :

		# plugs unrelated to referencing shouldn't disappear when a reference is
		# reloaded. various parts of the ui might be using them for other purposes.

		s = Gaffer.ScriptNode()

		s["n1"] = GafferTest.AddNode()
		s["n2"] = GafferTest.AddNode()
		s["n2"]["op1"].setInput( s["n1"]["sum"] )

		b = Gaffer.Box.create( s, Gaffer.StandardSet( [ s["n1"] ] ) )
		b.exportForReference( self.temporaryDirectory() + "/test.grf" )

		s2 = Gaffer.ScriptNode()
		s2["r"] = Gaffer.Reference()
		s2["r"].load( self.temporaryDirectory() + "/test.grf" )

		s2["r"]["__mySpecialPlug"] = Gaffer.IntPlug( flags = Gaffer.Plug.Flags.Default | Gaffer.Plug.Flags.Dynamic )

		s2["r"].load( self.temporaryDirectory() + "/test.grf" )

		self.assertTrue( "__mySpecialPlug" in s2["r"] )

	def testLoadScriptWithReference( self ) :

		s = Gaffer.ScriptNode()
		s["n1"] = GafferTest.AddNode()
		s["n2"] = GafferTest.AddNode()
		s["n3"] = GafferTest.AddNode()
		s["n2"]["op1"].setInput( s["n1"]["sum"] )
		s["n3"]["op1"].setInput( s["n2"]["sum"] )

		b = Gaffer.Box.create( s, Gaffer.StandardSet( [ s["n2"] ] ) )
		b.promotePlug( b["n2"]["op2"] )
		b.exportForReference( self.temporaryDirectory() + "/test.grf" )

		s2 = Gaffer.ScriptNode()
		s2["r"] = Gaffer.Reference()
		s2["r"].load( self.temporaryDirectory() + "/test.grf" )
		s2["a"] = GafferTest.AddNode()

		s2["r"]["op2"].setValue( 123 )
		s2["r"]["op1"].setInput( s2["a"]["sum"] )

		self.assertTrue( "n2" in s2["r"] )
		self.assertTrue( "sum" in s2["r"] )
		self.assertTrue( s2["r"]["op1"].getInput().isSame( s2["a"]["sum"] ) )

		s2["fileName"].setValue( self.temporaryDirectory() + "/test.gfr" )
		s2.save()

		s3 = Gaffer.ScriptNode()
		s3["fileName"].setValue( self.temporaryDirectory() + "/test.gfr" )
		s3.load()

		self.assertEqual( s3["r"].keys(), s2["r"].keys() )
		self.assertEqual( s3["r"]["user"].keys(), s2["r"]["user"].keys() )
		self.assertEqual( s3["r"]["op2"].getValue(), 123 )
		self.assertTrue( s3["r"]["op1"].getInput().isSame( s3["a"]["sum"] ) )

	def testReferenceExportCustomPlugsFromBoxes( self ) :

		s = Gaffer.ScriptNode()
		s["n1"] = GafferTest.AddNode()

		b = Gaffer.Box.create( s, Gaffer.StandardSet( [ s["n1"] ] ) )
		b["myCustomPlug"] = Gaffer.IntPlug( flags = Gaffer.Plug.Flags.Default | Gaffer.Plug.Flags.Dynamic )
		b["__invisiblePlugThatShouldntGetExported"] = Gaffer.IntPlug( flags = Gaffer.Plug.Flags.Default | Gaffer.Plug.Flags.Dynamic )

		b.exportForReference( self.temporaryDirectory() + "/test.grf" )

		s2 = Gaffer.ScriptNode()
		s2["r"] = Gaffer.Reference()
		s2["r"].load( self.temporaryDirectory() + "/test.grf" )

		self.assertTrue( "myCustomPlug" in s2["r"] )
		self.assertTrue( "__invisiblePlugThatShouldntGetExported" not in s2["r"] )

	def testNoExportOfUserPlugsFromBoxes( self ) :

		s = Gaffer.ScriptNode()

		s["b"] = Gaffer.Box()
		s["b"]["user"]["p"] = Gaffer.IntPlug( flags = Gaffer.Plug.Flags.Default | Gaffer.Plug.Flags.Dynamic )
		s["b"].exportForReference( self.temporaryDirectory() + "/test.grf" )

		s["r"] = Gaffer.Reference()
		s["r"].load( self.temporaryDirectory() + "/test.grf" )

		self.assertEqual( len( s["r"]["user"] ), 0 )

	def testLoadKeepsUserPlugs( self ) :

		s = Gaffer.ScriptNode()

		s["b"] = Gaffer.Box()
		s["b"].exportForReference( self.temporaryDirectory() + "/test.grf" )

		s["r"] = Gaffer.Reference()
		s["r"]["user"]["p"] = Gaffer.IntPlug( flags = Gaffer.Plug.Flags.Default | Gaffer.Plug.Flags.Dynamic )
		self.assertEqual( s["r"]["user"]["p"].getFlags(), Gaffer.Plug.Flags.Default | Gaffer.Plug.Flags.Dynamic )

		s["r"].load( self.temporaryDirectory() + "/test.grf" )
		self.assertTrue( "p" in s["r"]["user"] )
		self.assertEqual( s["r"]["user"]["p"].getFlags(), Gaffer.Plug.Flags.Default | Gaffer.Plug.Flags.Dynamic )

		s["fileName"].setValue( self.temporaryDirectory() + "/test.gfr" )
		s.save()

		s2 = Gaffer.ScriptNode()
		s2.execute( s.serialise() )
		self.assertTrue( "p" in s2["r"]["user"] )

	def testPlugMetadata( self ) :

		s = Gaffer.ScriptNode()
		s["n1"] = GafferTest.AddNode()

		b = Gaffer.Box.create( s, Gaffer.StandardSet( [ s["n1"] ] ) )
		p = b.promotePlug( b["n1"]["op1"] )

		Gaffer.Metadata.registerPlugValue( p, "description", "ppp" )

		b.exportForReference( self.temporaryDirectory() + "/test.grf" )

		s2 = Gaffer.ScriptNode()
		s2["r"] = Gaffer.Reference()
		s2["r"].load( self.temporaryDirectory() + "/test.grf" )

		self.assertEqual( Gaffer.Metadata.plugValue( s2["r"].descendant( p.relativeName( b ) ), "description" ), "ppp" )

	def testMetadataIsntResaved( self ) :

		s = Gaffer.ScriptNode()
		s["n1"] = GafferTest.AddNode()

		b = Gaffer.Box.create( s, Gaffer.StandardSet( [ s["n1"] ] ) )
		p = b.promotePlug( b["n1"]["op1"] )

		Gaffer.Metadata.registerPlugValue( p, "description", "ppp" )

		b.exportForReference( self.temporaryDirectory() + "/test.grf" )

		s2 = Gaffer.ScriptNode()
		s2["r"] = Gaffer.Reference()
		s2["r"].load( self.temporaryDirectory() + "/test.grf" )

		self.assertTrue( "Metadata" not in s2.serialise() )

	def testSinglePlugWithMetadata( self ) :

		s = Gaffer.ScriptNode()
		s["b"] = Gaffer.Box()
		s["b"]["p"] = Gaffer.Plug( flags = Gaffer.Plug.Flags.Default | Gaffer.Plug.Flags.Dynamic )

		Gaffer.Metadata.registerPlugValue( s["b"]["p"], "description", "ddd" )

		s["b"].exportForReference( self.temporaryDirectory() + "/test.grf" )

		s["r"] = Gaffer.Reference()
		s["r"].load( self.temporaryDirectory() + "/test.grf" )

		self.assertEqual( Gaffer.Metadata.plugValue( s["r"]["p"], "description" ), "ddd" )

	def testReloadWithUnconnectedPlugs( self ) :

		s = Gaffer.ScriptNode()
		s["b"] = Gaffer.Box()
		s["b"]["p"] = Gaffer.Plug( flags = Gaffer.Plug.Flags.Default | Gaffer.Plug.Flags.Dynamic )
		s["b"].exportForReference( self.temporaryDirectory() + "/test.grf" )

		s["r"] = Gaffer.Reference()
		s["r"].load( self.temporaryDirectory() + "/test.grf" )

		self.assertTrue( "p" in s["r"] )

		s2 = Gaffer.ScriptNode()
		s2.execute( s.serialise() )

		self.assertTrue( "p" in s2["r"] )

	def testReloadRefreshesMetadata( self ) :

		s = Gaffer.ScriptNode()
		s["b"] = Gaffer.Box()
		s["b"]["p"] = Gaffer.Plug( flags = Gaffer.Plug.Flags.Default | Gaffer.Plug.Flags.Dynamic )
		s["b"].exportForReference( self.temporaryDirectory() + "/test.grf" )

		s["r"] = Gaffer.Reference()
		s["r"].load( self.temporaryDirectory() + "/test.grf" )

		self.assertEqual( Gaffer.Metadata.plugValue( s["r"]["p"], "test" ), None )

		Gaffer.Metadata.registerPlugValue( s["b"]["p"], "test", 10 )
		s["b"].exportForReference( self.temporaryDirectory() + "/test.grf" )

		s["r"].load( self.temporaryDirectory() + "/test.grf" )

		self.assertEqual( Gaffer.Metadata.plugValue( s["r"]["p"], "test" ), 10 )

	def testDefaultValueClashes( self ) :

		# export a reference where a promoted plug is not at
		# its default value.

		s = Gaffer.ScriptNode()
		s["b"] = Gaffer.Box()
		s["b"]["n"] = GafferTest.AddNode()
		p = s["b"].promotePlug( s["b"]["n"]["op1"] )
		p.setValue( 10 )

		s["b"].exportForReference( self.temporaryDirectory() + "/test.grf" )

		# reference it in to a new script, set the value back to
		# its default, and save the script.

		s2 = Gaffer.ScriptNode()
		s2["r"] = Gaffer.Reference()
		s2["r"].load( self.temporaryDirectory() + "/test.grf" )

		p2 = s2["r"].descendant( p.relativeName( s["b"] ) )
		self.assertEqual( p2.getValue(), 10 )
		p2.setToDefault()
		self.assertEqual( p2.getValue(), p2.defaultValue() )

		s2["fileName"].setValue( self.temporaryDirectory() + "/test.gfr" )
		s2.save()

		# load the script, and check that the value is at the default.

		s3 = Gaffer.ScriptNode()
		s3["fileName"].setValue( self.temporaryDirectory() + "/test.gfr" )
		s3.load()

		p3 = s3["r"].descendant( p.relativeName( s["b"] ) )
		self.assertEqual( p3.getValue(), p3.defaultValue() )

	def testLoadThrowsExceptionsOnError( self ) :

		s = Gaffer.ScriptNode()
		s["b"] = Gaffer.Box()
		s["b"]["n"] = GafferTest.SphereNode()

		s["b"].exportForReference( self.temporaryDirectory() + "/test.grf" )

		del GafferTest.SphereNode # induce a failure during loading

		s2 = Gaffer.ScriptNode()
		s2["r"] = Gaffer.Reference()

		self.assertRaises( Exception, s2["r"].load, self.temporaryDirectory() + "/test.grf" )

	def testErrorTolerantLoading( self ) :

		# make a box containing 2 nodes, and export it.

		s = Gaffer.ScriptNode()
		s["b"] = Gaffer.Box()
		s["b"]["s"] = GafferTest.SphereNode()
		s["b"]["a"] = GafferTest.AddNode()

		s["b"].exportForReference( self.temporaryDirectory() + "/test.grf" )

		# import it into a script.

		s2 = Gaffer.ScriptNode()
		s2["r"] = Gaffer.Reference()
		s2["r"].load( self.temporaryDirectory() + "/test.grf" )

		self.assertTrue( "a" in s2["r"] )
		self.assertTrue( isinstance( s2["r"]["a"], GafferTest.AddNode ) )

		# save that script, and then mysteriously
		# disable GafferTest.SphereNode.

		s2["fileName"].setValue( self.temporaryDirectory() + "/test.gfr" )
		s2.save()

		del GafferTest.SphereNode

		# load the script, and check that we could at least
		# load in the other referenced node.

		s3 = Gaffer.ScriptNode()
		s3["fileName"].setValue( self.temporaryDirectory() + "/test.gfr" )
		with IECore.CapturingMessageHandler() as mh :
			s3.load( continueOnError=True )

		self.assertTrue( len( mh.messages ) )

		self.assertTrue( "a" in s3["r"] )
		self.assertTrue( isinstance( s3["r"]["a"], GafferTest.AddNode ) )

	def testDependencyNode( self ) :

		s = Gaffer.ScriptNode()

		# Make a reference, and check it's a DependencyNode

		s["r"] = Gaffer.Reference()
		self.assertTrue( isinstance( s["r"], Gaffer.DependencyNode ) )
		self.assertTrue( s["r"].isInstanceOf( Gaffer.DependencyNode.staticTypeId() ) )
		self.assertTrue( isinstance( s["r"], Gaffer.SubGraph ) )
		self.assertTrue( s["r"].isInstanceOf( Gaffer.SubGraph.staticTypeId() ) )

		# create a box with a promoted output:
		s["b"] = Gaffer.Box()
		s["b"]["n"] = GafferTest.AddNode()
		s["b"].promotePlug( s["b"]["n"]["sum"] )
		s["b"].exportForReference( self.temporaryDirectory() + "/test.grf" )

		# load onto reference:
		s["r"].load( self.temporaryDirectory() + "/test.grf" )
		self.assertEqual( s["r"].correspondingInput( s["r"]["sum"] ), None )
		self.assertEqual( s["r"].enabledPlug(), None )

		# Wire it up to support enabledPlug() and correspondingInput()
		s["b"].promotePlug( s["b"]["n"]["op1"] )
		s["b"]["n"]["op2"].setValue( 10 )
		s["b"].exportForReference( self.temporaryDirectory() + "/test.grf" )

		# reload reference and test:
		s["r"].load( self.temporaryDirectory() + "/test.grf" )
		self.assertEqual( s["r"].correspondingInput( s["r"]["sum"] ), None )
		self.assertEqual( s["r"].enabledPlug(), None )

		# add an enabled plug:
		s["b"]["enabled"] = Gaffer.BoolPlug( flags = Gaffer.Plug.Flags.Default | Gaffer.Plug.Flags.Dynamic )
		s["b"].exportForReference( self.temporaryDirectory() + "/test.grf" )

		# reload reference and test that's now visible via enabledPlug():
		s["r"].load( self.temporaryDirectory() + "/test.grf" )
		self.assertEqual( s["r"].correspondingInput( s["r"]["sum"] ), None )
		self.assertTrue( s["r"].enabledPlug().isSame( s["r"]["enabled"] ) )

		# hook up the enabled plug inside the box:
		s["b"]["n"]["enabled"].setInput( s["b"]["enabled"] )
		s["b"].exportForReference( self.temporaryDirectory() + "/test.grf" )

		# reload reference and test that's now visible via enabledPlug():
		s["r"].load( self.temporaryDirectory() + "/test.grf" )
		self.assertTrue( s["r"].enabledPlug().isSame( s["r"]["enabled"] ) )
		self.assertTrue( s["r"].correspondingInput( s["r"]["sum"] ).isSame( s["r"]["op1"] ) )


		# Connect it into a network, delete it, and check that we get nice auto-reconnect behaviour
		s["a"] = GafferTest.AddNode()
		s["r"]["op1"].setInput( s["a"]["sum"] )

		s["c"] = GafferTest.AddNode()
		s["c"]["op1"].setInput( s["r"]["sum"] )

		s.deleteNodes( filter = Gaffer.StandardSet( [ s["r"] ] ) )

		self.assertTrue( s["c"]["op1"].getInput().isSame( s["a"]["sum"] ) )

	def testPlugFlagsOnReload( self ):

		s = Gaffer.ScriptNode()
		s["b"] = Gaffer.Box()
		s["b"]["s"] = GafferTest.SphereNode()
		s["b"]["a"] = GafferTest.AddNode()

		s["b"].exportForReference( self.temporaryDirectory() + "/test.grf" )

		# import it into a script.

		s2 = Gaffer.ScriptNode()
		s2["r"] = Gaffer.Reference()
		s2["r"].load( self.temporaryDirectory() + "/test.grf" )
		s2["r"]["__pluggy"] = Gaffer.CompoundPlug( flags = Gaffer.Plug.Flags.Dynamic | Gaffer.Plug.Flags.Default )
		s2["r"]["__pluggy"]["int"] = Gaffer.IntPlug( flags = Gaffer.Plug.Flags.Dynamic | Gaffer.Plug.Flags.Default )
		s2["r"]["__pluggy"]["compound"] = Gaffer.CompoundPlug( flags = Gaffer.Plug.Flags.Dynamic | Gaffer.Plug.Flags.Default )
		s2["r"]["__pluggy"]["compound"]["int"] = Gaffer.IntPlug( flags = Gaffer.Plug.Flags.Dynamic | Gaffer.Plug.Flags.Default )

		self.assertEqual( s2["r"]["__pluggy"].getFlags(), Gaffer.Plug.Flags.Dynamic | Gaffer.Plug.Flags.Default )
		self.assertEqual( s2["r"]["__pluggy"]["int"].getFlags(), Gaffer.Plug.Flags.Dynamic | Gaffer.Plug.Flags.Default )
		self.assertEqual( s2["r"]["__pluggy"]["compound"].getFlags(), Gaffer.Plug.Flags.Dynamic | Gaffer.Plug.Flags.Default )
		self.assertEqual( s2["r"]["__pluggy"]["compound"]["int"].getFlags(), Gaffer.Plug.Flags.Dynamic | Gaffer.Plug.Flags.Default )

		s2["r"].load( self.temporaryDirectory() + "/test.grf" )

		self.assertEqual( s2["r"]["__pluggy"].getFlags(), Gaffer.Plug.Flags.Dynamic | Gaffer.Plug.Flags.Default )
		self.assertEqual( s2["r"]["__pluggy"]["int"].getFlags(), Gaffer.Plug.Flags.Dynamic | Gaffer.Plug.Flags.Default )
		self.assertEqual( s2["r"]["__pluggy"]["compound"].getFlags(), Gaffer.Plug.Flags.Dynamic | Gaffer.Plug.Flags.Default )
		self.assertEqual( s2["r"]["__pluggy"]["compound"]["int"].getFlags(), Gaffer.Plug.Flags.Dynamic | Gaffer.Plug.Flags.Default )

	def testDefaultValues( self ) :

		s = Gaffer.ScriptNode()
		s["b"] = Gaffer.Box()
		s["b"]["p"] = Gaffer.IntPlug( defaultValue = 1, flags = Gaffer.Plug.Flags.Default | Gaffer.Plug.Flags.Dynamic )
		s["b"]["p"].setValue( 2 )
		s["b"].exportForReference( self.temporaryDirectory() + "/test.grf" )

		s["r"] = Gaffer.Reference()
		s["r"].load( self.temporaryDirectory() + "/test.grf" )

		# The value at the time of box export should become
		# the default value on the reference node. But the
		# default value on the box itself should remain the
		# same.

		self.assertEqual( s["r"]["p"].getValue(), 2 )
		self.assertEqual( s["r"]["p"].defaultValue(), 2 )
		self.assertEqual( s["b"]["p"].defaultValue(), 1 )

		# And we should be able to save and reload the script
		# and have that still be the case.

		s["fileName"].setValue( self.temporaryDirectory() + "/test.gfr" )
		s.save()
		s.load()

		self.assertEqual( s["r"]["p"].getValue(), 2 )
		self.assertEqual( s["r"]["p"].defaultValue(), 2 )
		self.assertEqual( s["b"]["p"].getValue(), 2 )
		self.assertEqual( s["b"]["p"].defaultValue(), 1 )

		# If we change the value on the box and reexport,
		# then the reference should pick up both the new value
		# and the new default.

		s["b"]["p"].setValue( 3 )
		s["b"].exportForReference( self.temporaryDirectory() + "/test.grf" )
		s["r"].load( self.temporaryDirectory() + "/test.grf" )

		self.assertEqual( s["r"]["p"].getValue(), 3 )
		self.assertEqual( s["r"]["p"].defaultValue(), 3 )
		self.assertEqual( s["b"]["p"].getValue(), 3 )
		self.assertEqual( s["b"]["p"].defaultValue(), 1 )

		# And that should still hold after saving and reloading the script.

		s.save()
		s.load()
		self.assertEqual( s["r"]["p"].getValue(), 3 )
		self.assertEqual( s["r"]["p"].defaultValue(), 3 )
		self.assertEqual( s["b"]["p"].getValue(), 3 )
		self.assertEqual( s["b"]["p"].defaultValue(), 1 )

		# But if the user changes the value on the reference node,
		# it should be kept.

		s["r"]["p"].setValue( 100 )

		self.assertEqual( s["r"]["p"].getValue(), 100 )
		self.assertEqual( s["r"]["p"].defaultValue(), 3 )
		self.assertEqual( s["b"]["p"].getValue(), 3 )
		self.assertEqual( s["b"]["p"].defaultValue(), 1 )

		# And a save and load shouldn't change that.

		s.save()
		s.load()

		self.assertEqual( s["r"]["p"].getValue(), 100 )
		self.assertEqual( s["r"]["p"].defaultValue(), 3 )
		self.assertEqual( s["b"]["p"].getValue(), 3 )
		self.assertEqual( s["b"]["p"].defaultValue(), 1 )

		# And now the user has changed a value, only the
		# default value should be updated if we load a new
		# reference.

		s["b"]["p"].setValue( 4 )
		s["b"].exportForReference( self.temporaryDirectory() + "/test.grf" )
		s["r"].load( self.temporaryDirectory() + "/test.grf" )

		self.assertEqual( s["r"]["p"].getValue(), 100 )
		self.assertEqual( s["r"]["p"].defaultValue(), 4 )
		self.assertEqual( s["b"]["p"].getValue(), 4 )
		self.assertEqual( s["b"]["p"].defaultValue(), 1 )

		# And a save and load shouldn't change anything.

		s.save()
		s.load()

		self.assertEqual( s["r"]["p"].getValue(), 100 )
		self.assertEqual( s["r"]["p"].defaultValue(), 4 )
		self.assertEqual( s["b"]["p"].getValue(), 4 )
		self.assertEqual( s["b"]["p"].defaultValue(), 1 )

		# And since we know that all plugs in box exports
		# have had their default values set to the current
		# value, there shouldn't be any need for a single
		# setValue() call in the exported file.

		e = "".join( file( self.temporaryDirectory() + "/test.grf" ).readlines() )
		self.assertTrue( "setValue" not in e )

	def testInternalNodeDefaultValues( self ) :

		s = Gaffer.ScriptNode()
		s["b"] = Gaffer.Box()
		s["b"]["n"] = Gaffer.Node()
		s["b"]["n"]["p"] = Gaffer.IntPlug( defaultValue = 1, flags = Gaffer.Plug.Flags.Default | Gaffer.Plug.Flags.Dynamic )
		s["b"]["n"]["p"].setValue( 2 )
		s["b"].exportForReference( self.temporaryDirectory() + "/test.grf" )

		s["r"] = Gaffer.Reference()
		s["r"].load( self.temporaryDirectory() + "/test.grf" )

		# Nothing at all should have changed about the
		# values and defaults on the internal nodes.

		self.assertEqual( s["r"]["n"]["p"].getValue(), 2 )
		self.assertEqual( s["r"]["n"]["p"].defaultValue(), 1 )

		# And we should be able to save and reload the script
		# and have that still be the case.

		s["fileName"].setValue( self.temporaryDirectory() + "/test.gfr" )
		s.save()
		s.load()

		self.assertEqual( s["r"]["n"]["p"].getValue(), 2 )
		self.assertEqual( s["r"]["n"]["p"].defaultValue(), 1 )

	def testNodeMetadata( self ) :

		s = Gaffer.ScriptNode()
		s["b"] = Gaffer.Box()

		Gaffer.Metadata.registerNodeValue( s["b"], "description", "Test description" )
		Gaffer.Metadata.registerNodeValue( s["b"], "nodeGadget:color", IECore.Color3f( 1, 0, 0 ) )

		s["b"].exportForReference( self.temporaryDirectory() + "/test.grf" )

		s["r"] = Gaffer.Reference()
		s["r"].load( self.temporaryDirectory() + "/test.grf" )

		self.assertEqual( Gaffer.Metadata.nodeValue( s["r"], "description" ), "Test description" )
		self.assertEqual( Gaffer.Metadata.nodeValue( s["r"], "nodeGadget:color" ), IECore.Color3f( 1, 0, 0 ) )

	def testVersionMetadata( self ) :

		s = Gaffer.ScriptNode()
		s["b"] = Gaffer.Box()
		s["b"].exportForReference( self.temporaryDirectory() + "/test.grf" )

		s["r"] = Gaffer.Reference()
		s["r"].load( self.temporaryDirectory() + "/test.grf" )

		self.assertEqual( Gaffer.Metadata.nodeValue( s["r"], "serialiser:milestoneVersion" ), Gaffer.About.milestoneVersion() )
		self.assertEqual( Gaffer.Metadata.nodeValue( s["r"], "serialiser:majorVersion" ), Gaffer.About.majorVersion() )
		self.assertEqual( Gaffer.Metadata.nodeValue( s["r"], "serialiser:minorVersion" ), Gaffer.About.minorVersion() )
		self.assertEqual( Gaffer.Metadata.nodeValue( s["r"], "serialiser:patchVersion" ), Gaffer.About.patchVersion() )

		self.assertTrue( "serialiser:milestoneVersion" not in Gaffer.Metadata.registeredNodeValues( s["r"], persistentOnly = True ) )
		self.assertTrue( "serialiser:majorVersion" not in Gaffer.Metadata.registeredNodeValues( s["r"], persistentOnly = True ) )
		self.assertTrue( "serialiser:minorVersion" not in Gaffer.Metadata.registeredNodeValues( s["r"], persistentOnly = True ) )
		self.assertTrue( "serialiser:patchVersion" not in Gaffer.Metadata.registeredNodeValues( s["r"], persistentOnly = True ) )

	def testBackwardCompatibility( self ) :

		s = Gaffer.ScriptNode()
		s["r"] = Gaffer.Reference()
		s["r"].load( os.path.dirname( __file__ ) + "/references/version-0.8.0.0.grf" )

		self.assertEqual( s["r"]["user"]["promoted"].defaultValue(), False )
		self.assertEqual( s["r"]["user"]["promoted"].getValue(), True )

		s["r"]["user"]["promoted"].setValue( False )
		self.assertEqual( s["r"]["user"]["promoted"].defaultValue(), False )
		self.assertEqual( s["r"]["user"]["promoted"].getValue(), False )

		s2 = Gaffer.ScriptNode()
		s2.execute( s.serialise() )

		self.assertEqual( s2["r"]["user"]["promoted"].defaultValue(), False )
		self.assertEqual( s2["r"]["user"]["promoted"].getValue(), False )

	def testCutAndPasteOldReferenceTwice( self ) :

		s = Gaffer.ScriptNode()
		s["r"] = Gaffer.Reference()
		s["r"].load( os.path.dirname( __file__ ) + "/references/version-0.8.0.0.grf" )

		self.assertEqual( s["r"]["user"]["promoted"].defaultValue(), False )
		self.assertEqual( s["r"]["user"]["promoted"].getValue(), True )

		s["r"]["user"]["promoted"].setValue( False )
		self.assertEqual( s["r"]["user"]["promoted"].defaultValue(), False )
		self.assertEqual( s["r"]["user"]["promoted"].getValue(), False )

		s.execute( s.serialise( filter = Gaffer.StandardSet( [ s["r"] ] ) ) )

		self.assertEqual( s["r1"]["user"]["promoted"].defaultValue(), False )
		self.assertEqual( s["r1"]["user"]["promoted"].getValue(), False )

		s.execute( s.serialise( filter = Gaffer.StandardSet( [ s["r1"] ] ) ) )

		self.assertEqual( s["r2"]["user"]["promoted"].defaultValue(), False )
		self.assertEqual( s["r2"]["user"]["promoted"].getValue(), False )

	def testReloadOldReference( self ) :

		s = Gaffer.ScriptNode()
		s["r"] = Gaffer.Reference()
		s["r"].load( os.path.dirname( __file__ ) + "/references/version-0.8.0.0.grf" )

		self.assertEqual( s["r"]["user"]["promoted"].defaultValue(), False )
		self.assertEqual( s["r"]["user"]["promoted"].getValue(), True )

		s["r"]["user"]["promoted"].setValue( False )
		self.assertEqual( s["r"]["user"]["promoted"].defaultValue(), False )
		self.assertEqual( s["r"]["user"]["promoted"].getValue(), False )

		s["r"].load( os.path.dirname( __file__ ) + "/references/version-0.8.0.0.grf" )
		self.assertEqual( s["r"]["user"]["promoted"].defaultValue(), False )
		self.assertEqual( s["r"]["user"]["promoted"].getValue(), False )

	def testSerialiseWithoutLoading( self ) :

		s = Gaffer.ScriptNode()
		s["r"] = Gaffer.Reference()

		s2 = Gaffer.ScriptNode()
		s2.execute( s.serialise() )

	def testUserPlugMetadata( self ) :

		# People should be able to do what they want with the user plug,
		# and anything they do should be serialised appropriately.

		s = Gaffer.ScriptNode()
		s["r"] = Gaffer.Reference()
		s["r"]["user"]["p"] = Gaffer.IntPlug( flags = Gaffer.Plug.Flags.Default | Gaffer.Plug.Flags.Dynamic )

		Gaffer.Metadata.registerPlugValue( s["r"]["user"], "testPersistent", 1, persistent = True )
		Gaffer.Metadata.registerPlugValue( s["r"]["user"], "testNonPersistent", 2, persistent = False )

		Gaffer.Metadata.registerPlugValue( s["r"]["user"]["p"], "testPersistent", 3, persistent = True )
		Gaffer.Metadata.registerPlugValue( s["r"]["user"]["p"], "testNonPersistent", 4, persistent = False )

		self.assertEqual( Gaffer.Metadata.plugValue( s["r"]["user"], "testPersistent" ), 1 )
		self.assertEqual( Gaffer.Metadata.plugValue( s["r"]["user"], "testNonPersistent" ), 2 )

		self.assertEqual( Gaffer.Metadata.plugValue( s["r"]["user"]["p"], "testPersistent" ), 3 )
		self.assertEqual( Gaffer.Metadata.plugValue( s["r"]["user"]["p"], "testNonPersistent" ), 4 )

		s2 = Gaffer.ScriptNode()
		s2.execute( s.serialise() )

		self.assertEqual( Gaffer.Metadata.plugValue( s2["r"]["user"], "testPersistent" ), 1 )
		self.assertEqual( Gaffer.Metadata.plugValue( s2["r"]["user"], "testNonPersistent" ), None )

		self.assertEqual( Gaffer.Metadata.plugValue( s2["r"]["user"]["p"], "testPersistent" ), 3 )
		self.assertEqual( Gaffer.Metadata.plugValue( s2["r"]["user"]["p"], "testNonPersistent" ), None )

	def testNamespaceIsClear( self ) :

		# We need the namespace of the node to be empty, so
		# that people can call plugs anything they want when
		# authoring references.

		r = Gaffer.Reference()
		n = Gaffer.Node()
		self.assertEqual( r.keys(), n.keys() )

	def testPlugCalledFileName( self ) :

		s = Gaffer.ScriptNode()

		s["b"] = Gaffer.Box()
		s["b"]["fileName"] = Gaffer.StringPlug( flags = Gaffer.Plug.Flags.Default | Gaffer.Plug.Flags.Dynamic )
		s["b"]["fileName"].setValue( "iAmUsingThisForMyOwnPurposes" )
		s["b"].exportForReference( self.temporaryDirectory() + "/test.grf" )

		s["r"] = Gaffer.Reference()
		s["r"].load( self.temporaryDirectory() + "/test.grf" )

		self.assertEqual( s["r"]["fileName"].getValue(), "iAmUsingThisForMyOwnPurposes" )

	def testLoadScriptWithReferenceFromVersion0_14( self ) :

		shutil.copyfile(
			os.path.dirname( __file__ ) + "/references/version-0.14.0.0.grf",
			"/tmp/test.grf"
		)

		s = Gaffer.ScriptNode()
		s["fileName"].setValue( os.path.dirname( __file__ ) + "/scripts/referenceVersion-0.14.0.0.gfr" )

		with IECore.CapturingMessageHandler() as mh :
			s.load( continueOnError = True )

		# Although we expect it to load OK, we do also expect to receive a
		# warning message because we removed the fileName plug after version 0.14.
		self.assertEqual( len( mh.messages ), 1 )
		self.assertTrue( "KeyError: 'fileName'" in mh.messages[0].message )

		self.assertEqual( s["Reference"]["testPlug"].getValue(), 2 )

	def testFileNameAccessor( self ) :

		s = Gaffer.ScriptNode()
		s["b"] = Gaffer.Box()
		s["b"]["p"] = Gaffer.Plug( flags = Gaffer.Plug.Flags.Default | Gaffer.Plug.Flags.Dynamic )
		s["b"].exportForReference( self.temporaryDirectory() + "/test.grf" )

		s["r"] = Gaffer.Reference()
		self.assertEqual( s["r"].fileName(), "" )

		s["r"].load( self.temporaryDirectory() + "/test.grf" )
		self.assertEqual( s["r"].fileName(), self.temporaryDirectory() + "/test.grf" )

	def testUndo( self ) :

		s = Gaffer.ScriptNode()
		s["b"] = Gaffer.Box()
		s["b"]["p"] = Gaffer.Plug( flags = Gaffer.Plug.Flags.Default | Gaffer.Plug.Flags.Dynamic )
		s["b"].exportForReference( self.temporaryDirectory() + "/test.grf" )

		s["r"] = Gaffer.Reference()
		self.assertEqual( s["r"].fileName(), "" )
		self.assertTrue( "p" not in s["r"] )

		State = collections.namedtuple( "State", [ "keys", "fileName" ] )
		states = []
		def referenceLoaded( node ) :
			states.append( State( keys = node.keys(), fileName = node.fileName() ) )

		c = s["r"].referenceLoadedSignal().connect( referenceLoaded )

		with Gaffer.UndoContext( s ) :
			s["r"].load( self.temporaryDirectory() + "/test.grf" )

		self.assertTrue( "p" in s["r"] )
		self.assertEqual( s["r"].fileName(), self.temporaryDirectory() + "/test.grf" )
		self.assertTrue( len( states ), 1 )
		self.assertEqual( states[0], State( [ "user", "p" ], self.temporaryDirectory() + "/test.grf" ) )

		s.undo()
		self.assertEqual( s["r"].fileName(), "" )
		self.assertTrue( "p" not in s["r"] )
		self.assertTrue( len( states ), 2 )
		self.assertEqual( states[1], State( [ "user" ], "" ) )

		s.redo()
		self.assertTrue( "p" in s["r"] )
		self.assertEqual( s["r"].fileName(), self.temporaryDirectory() + "/test.grf" )
		self.assertTrue( len( states ), 3 )
		self.assertEqual( states[2], State( [ "user", "p" ], self.temporaryDirectory() + "/test.grf" ) )

	def testHasEdit( self ) :

		s = Gaffer.ScriptNode()
		s["b"] = Gaffer.Box()
		s["b"]["p1"] = Gaffer.IntPlug( flags = Gaffer.Plug.Flags.Default | Gaffer.Plug.Flags.Dynamic )
		s["b"]["p2"] = Gaffer.IntPlug( flags = Gaffer.Plug.Flags.Default | Gaffer.Plug.Flags.Dynamic )
		s["b"].exportForReference( self.temporaryDirectory() + "/test.grf" )

		s["r"] = Gaffer.Reference()
		s["r"].load( self.temporaryDirectory() + "/test.grf" )

		self.assertFalse( s["r"].hasEdit( s["r"]["p1"] ) )
		self.assertFalse( s["r"].hasEdit( s["r"]["p2"] ) )

		with Gaffer.UndoContext( s ) :
			s["r"]["p1"].setValue( 10 )

		self.assertTrue( s["r"].hasEdit( s["r"]["p1"] ) )
		self.assertFalse( s["r"].hasEdit( s["r"]["p2"] ) )

		with Gaffer.UndoContext( s ) :
			s["r"]["p1"].setValue( 20 )
			s["r"]["p2"].setValue( 30 )

		self.assertTrue( s["r"].hasEdit( s["r"]["p1"] ) )
		self.assertTrue( s["r"].hasEdit( s["r"]["p2"] ) )

		s.undo()

		self.assertTrue( s["r"].hasEdit( s["r"]["p1"] ) )
		self.assertFalse( s["r"].hasEdit( s["r"]["p2"] ) )

		s.undo()

		self.assertFalse( s["r"].hasEdit( s["r"]["p1"] ) )
		self.assertFalse( s["r"].hasEdit( s["r"]["p2"] ) )

		s.redo()

		self.assertTrue( s["r"].hasEdit( s["r"]["p1"] ) )
		self.assertFalse( s["r"].hasEdit( s["r"]["p2"] ) )

		s.redo()

		self.assertTrue( s["r"].hasEdit( s["r"]["p1"] ) )
		self.assertTrue( s["r"].hasEdit( s["r"]["p2"] ) )

	def testEditToDefaultValue( self ) :

		s = Gaffer.ScriptNode()
		s["b"] = Gaffer.Box()
		s["b"]["p"] = Gaffer.IntPlug( defaultValue = 1, flags = Gaffer.Plug.Flags.Default | Gaffer.Plug.Flags.Dynamic )
		s["b"].exportForReference( self.temporaryDirectory() + "/test.grf" )

		s["r"] = Gaffer.Reference()
		s["r"].load( self.temporaryDirectory() + "/test.grf" )
		self.assertFalse( s["r"].hasEdit( s["r"]["p"] ) )

		s["r"]["p"].setValue( 2 )
		self.assertTrue( s["r"].hasEdit( s["r"]["p"] ) )

		# If a plug has been explicitly set, it is considered
		# an edit even if the value is the default value.
		s["r"]["p"].setToDefault()
		self.assertTrue( s["r"].hasEdit( s["r"]["p"] ) )

		# So if we were to reexport the box with a new default
		# value, we would still be overriding it.

		s["b"]["p"] = Gaffer.IntPlug( defaultValue = 3, flags = Gaffer.Plug.Flags.Default | Gaffer.Plug.Flags.Dynamic )
		s["b"].exportForReference( self.temporaryDirectory() + "/test.grf" )

		s["r"].load( self.temporaryDirectory() + "/test.grf" )
		self.assertTrue( s["r"].hasEdit( s["r"]["p"] ) )
		self.assertEqual( s["r"]["p"].getValue(), 1 )

	def testHasEditAfterReload( self ) :

		s = Gaffer.ScriptNode()
		s["b"] = Gaffer.Box()
		s["b"]["p"] = Gaffer.IntPlug( defaultValue = 1, flags = Gaffer.Plug.Flags.Default | Gaffer.Plug.Flags.Dynamic )
		s["b"].exportForReference( self.temporaryDirectory() + "/test.grf" )

		s["r"] = Gaffer.Reference()
		s["r"].load( self.temporaryDirectory() + "/test.grf" )
		self.assertFalse( s["r"].hasEdit( s["r"]["p"] ) )

		s["r"]["p"].setValue( 2 )
		p = s["r"]["p"]
		self.assertTrue( s["r"].hasEdit( p ) )
		self.assertTrue( s["r"].hasEdit( s["r"]["p"] ) )

		s["r"].load( self.temporaryDirectory() + "/test.grf" )

		# The old plug doesn't have an edit any more, because
		# it doesn't even belong to the reference any more.
		self.assertFalse( s["r"].isAncestorOf( p ) )
		self.assertFalse( s["r"].hasEdit( p ) )

		# But the new plug does have one.
		self.assertTrue( s["r"].hasEdit( s["r"]["p"] ) )

	def testChangeDefaultToEditedValue( self ) :

		s = Gaffer.ScriptNode()
		s["b"] = Gaffer.Box()
		s["b"]["p"] = Gaffer.IntPlug( defaultValue = 1, flags = Gaffer.Plug.Flags.Default | Gaffer.Plug.Flags.Dynamic )
		s["b"].exportForReference( self.temporaryDirectory() + "/test.grf" )

		s["r"] = Gaffer.Reference()
		s["r"].load( self.temporaryDirectory() + "/test.grf" )
		s["r"]["p"].setValue( 2 )
		self.assertTrue( s["r"].hasEdit( s["r"]["p"] ) )

		# Reexport the box with a new default value that
		# happens to match the edit in the reference.

		s["b"]["p"] = Gaffer.IntPlug( defaultValue = 2, flags = Gaffer.Plug.Flags.Default | Gaffer.Plug.Flags.Dynamic )
		s["b"].exportForReference( self.temporaryDirectory() + "/test.grf" )

		# Reload the reference and check that it is still
		# considered to have an edit.

		s["r"].load( self.temporaryDirectory() + "/test.grf" )
		self.assertEqual( s["r"]["p"].defaultValue(), 2 )
		self.assertEqual( s["r"]["p"].getValue(), 2 )
		self.assertTrue( s["r"].hasEdit( s["r"]["p"] ) )

	def testEditingUndoCanBeMerged( self ) :

		s = Gaffer.ScriptNode()
		s["b"] = Gaffer.Box()
		s["b"]["p"] = Gaffer.IntPlug( flags = Gaffer.Plug.Flags.Default | Gaffer.Plug.Flags.Dynamic )
		s["b"].exportForReference( self.temporaryDirectory() + "/test.grf" )

		s["r"] = Gaffer.Reference()
		s["r"].load( self.temporaryDirectory() + "/test.grf" )
		self.assertEqual( s["r"]["p"].getValue(), 0 )

		with Gaffer.UndoContext( s, mergeGroup = "test" ) :
			s["r"]["p"].setValue( 1 )
			self.assertEqual( s["r"]["p"].getValue(), 1 )

		with Gaffer.UndoContext( s, mergeGroup = "test" ) :
			s["r"]["p"].setValue( 2 )
			self.assertEqual( s["r"]["p"].getValue(), 2 )

		with Gaffer.UndoContext( s, mergeGroup = "test" ) :
			s["r"]["p"].setValue( 3 )
			self.assertEqual( s["r"]["p"].getValue(), 3 )

		s.undo()

		self.assertFalse( s.undoAvailable() )
		self.assertEqual( s["r"]["p"].getValue(), 0 )

	def testEditsRemovedWhenPlugsDeleted( self ) :

		s = Gaffer.ScriptNode()
		s["b"] = Gaffer.Box()
		s["b"].exportForReference( self.temporaryDirectory() + "/withoutPlug.grf" )

		s["b"]["p"] = Gaffer.IntPlug( flags = Gaffer.Plug.Flags.Default | Gaffer.Plug.Flags.Dynamic )
		s["b"].exportForReference( self.temporaryDirectory() + "/withPlug.grf" )

		s["r"] = Gaffer.Reference()
		s["r"].load( self.temporaryDirectory() + "/withPlug.grf" )
		self.assertFalse( s["r"].hasEdit( s["r"]["p"] ) )

		s["r"]["p"].setValue( 10 )
		self.assertEqual( s["r"]["p"].getValue(), 10 )
		self.assertTrue( s["r"].hasEdit( s["r"]["p"] ) )

		s["r"].load( self.temporaryDirectory() + "/withoutPlug.grf" )
		s["r"].load( self.temporaryDirectory() + "/withPlug.grf" )

		self.assertEqual( s["r"]["p"].getValue(), 0 )
		self.assertFalse( s["r"].hasEdit( s["r"]["p"] ) )

	def testRemoveEdit( self ) :

		s = Gaffer.ScriptNode()
		s["b"] = Gaffer.Box()
		s["b"]["p"] = Gaffer.IntPlug( flags = Gaffer.Plug.Flags.Default | Gaffer.Plug.Flags.Dynamic )
		s["b"].exportForReference( self.temporaryDirectory() + "/test.grf" )

		s["r"] = Gaffer.Reference()
		s["r"].load( self.temporaryDirectory() + "/test.grf" )
		self.assertFalse( s["r"].hasEdit( s["r"]["p"] ) )
		self.assertEqual( s["r"]["p"].getValue(), 0 )

		with Gaffer.UndoContext( s ) :
			s["r"]["p"].setValue( 1 )
			self.assertTrue( s["r"].hasEdit( s["r"]["p"] ) )
			self.assertEqual( s["r"]["p"].getValue(), 1 )

		with Gaffer.UndoContext( s ) :
			s["r"].removeEdit( s["r"]["p"] )
			self.assertFalse( s["r"].hasEdit( s["r"]["p"] ) )
			self.assertEqual( s["r"]["p"].getValue(), 0 )

		s.undo()
		self.assertTrue( s["r"].hasEdit( s["r"]["p"] ) )
		self.assertEqual( s["r"]["p"].getValue(), 1 )

		s.undo()
		self.assertFalse( s["r"].hasEdit( s["r"]["p"] ) )
		self.assertEqual( s["r"]["p"].getValue(), 0 )

		s.redo()
		self.assertTrue( s["r"].hasEdit( s["r"]["p"] ) )
		self.assertEqual( s["r"]["p"].getValue(), 1 )

		s.redo()
		self.assertFalse( s["r"].hasEdit( s["r"]["p"] ) )
		self.assertEqual( s["r"]["p"].getValue(), 0 )

	def tearDown( self ) :

		GafferTest.TestCase.tearDown( self )

		GafferTest.SphereNode = self.__SphereNode

		if os.path.exists( "/tmp/test.grf" ) :
			os.remove( "/tmp/test.grf" )

if __name__ == "__main__":
	unittest.main()

