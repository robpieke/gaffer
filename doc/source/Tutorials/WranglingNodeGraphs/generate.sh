#! /bin/bash

set -e

gaffer screengrab \
	-script scripts/advancedSelection.gfr \
	-selection ShaderAssignment Group Sphere Plane3 Group1 Group2 \
	           Plane Plane1 Sphere1 Group3 Group4 Group5 Plane2 \
	           Sphere2 Sphere3 Group6 \
	-editor NodeGraph \
	-image images/upstreamSelection.png

gaffer screengrab \
	-script scripts/advancedSelection.gfr \
	-selection StandardAttributes StandardOptions Outputs AppleseedRender \
	           Outputs1 AppleseedRender1 SystemCommand ShaderAssignment \
	-editor NodeGraph \
	-image images/downstreamSelection.png

gaffer screengrab \
	-command "import Gaffer; script.addChild( Gaffer.Node() )" \
	-editor NodeGraph \
	-nodeGraph.contextMenu Node \
	-highlightedMenuItem "Set Color..." \
	-image images/nodeContextMenu.png

gaffer screengrab \
	-script scripts/colours.gfr \
	-editor NodeGraph \
	-image images/colours.png

gaffer screengrab \
	-script scripts/dotBefore.gfr \
	-selection SetFilter \
	-editor NodeGraph \
	-image images/dotBefore.png

gaffer screengrab \
	-script scripts/dotAfter.gfr \
	-selection Dot \
	-editor NodeGraph \
	-image images/dotAfter.png

gaffer screengrab \
	-script scripts/dotAfter.gfr \
	-command 'import Gaffer; script["Dot"]["labelType"].setValue( Gaffer.Dot.LabelType.UpstreamNodeName )' \
	-selection Dot \
	-nodeGraph.frame Dot \
	-editor NodeGraph \
	-image images/dotLabels.png

gaffer screengrab \
	-script scripts/bookmarks.gfr \
	-nodeGraph.frame as_color_texture \
	-editor NodeGraph \
	-nodeGraph.contextMenu as_color_texture \
	-highlightedMenuItem "Bookmarked" \
	-image images/bookmarkCreation.png

gaffer screengrab \
	-script scripts/teleportingBefore.gfr \
	-editor NodeGraph \
	-image images/teleportingBefore.png

gaffer screengrab \
	-script scripts/teleportingDot.gfr \
	-editor NodeGraph \
	-image images/teleportingDot.png

gaffer screengrab \
	-script scripts/teleportingDotInputsHidden.gfr \
	-editor NodeGraph \
	-nodeGraph.contextMenu Dot \
	-nodeGraph.frame Dot \
	-highlightedMenuItem "Show Input Connections" \
	-image images/teleportingMenu.png

gaffer screengrab \
	-script scripts/teleportingDotInputsHidden.gfr \
	-selection Dot \
	-editor NodeGraph \
	-image images/teleportingSelection.png

cp $GAFFER_ROOT/graphics/shading.png images
