Wrangling Node Graphs
=====================

It's not uncommon to find production node graphs containing tens of thousands of nodes, and keeping things organised isn't always easy at this scale. In this short tutorial we'll take a tour of a few features intended to help wrangle chaotic graphs into a vague sense of order.

Advanced Selection
------------------

When reorganising a graph, its often necessary to select all the nodes that are upstream or downstream from a particular point so you can move them to make space. This can be pretty tedious to do by hand, so Gaffer provides a shortcut in each case.

- To select all upstream nodes, _Shift+Alt Click_ on a node

![Upstream selection](images/upstreamSelection.png)

- To select all downsteam nodes, _Shift+Control Click_ on a node

![Downstream selection](images/downstreamSelection.png)

> Tip : You can also use the _/Edit/Select Connected_ menu items from the main menu
> to perform more fine grained selection of connected nodes.

Node Colours
------------

By default Gaffer's nodes are coloured according to type, but you can override the colour on a node-by-node basis. This can be used to highlight important nodes, or to visually group distant but related nodes.
Setting a node colour is pretty straightforward :

- Locate the node in the NodeGraph
- _Right-Click_ on the node to show the node context menu
- Choose the _/Set Color..._ menu item
- Use the colour picker to choose a colour

![Node context menu](images/nodeContextMenu.png)

Here we see an example use where a series of filters have been colour coded to associate them with the particular geometry they target :

![Color coded filters](images/colours.png)

> Tip : Assigning a colour to shader nodes makes it possible to use the Viewer's
> shader assignment diagnostic mode, accessed via the shading menu ![shading menu](images/shading.png)
> in the main Viewer toolbar. The shader's node colour is also used to identify it in the
> SceneInspector.

Dots
----

Connections in the NodeGraph take a direct route between their endpoints, and this can produce a cluttered graph where connections criss-cross or disappear underneath nodes. Dots are simple pass-through nodes that can be used to reroute connections to make them less confusing.

In the example below, the connection from the SetFilter passes awkwardly underneath the PathFilter, so the casual observer could be forgiven for thinking that the connection into ShaderAssignment2 came from the PathFilter. The highlighting of connections from the selected node helps somewhat with this, but it would be better to reroute the connection.

![Untidy connection](images/dotBefore.png)

Dots can be created via the node menu as usual (_/Utility/Dot_), but it's often more convenient to insert one directly into an existing connection :

- _Right click_ on the connection in the NodeGraph
- Choose the _Insert Dot_ menu item
- _Left Drag_ to place the dot where you want it

![Tidy connection](images/dotAfter.png)

By default, Dots don't display a label because you often don't need one and it's tedious to give them all sensible names. It's often more meaningful to label them with the name of the upstream node that they receive their connection from, or to give them a custom label. This is achieved by editing the label settings in the NodeEditor.

![Dot labels](images/dotLabels.png)

Bookmarks
---------

When working on large graphs, its common to find yourself repeatedly needing to make connections involving a few key nodes. And because the graph is large, its common for those nodes to be offscreen when you want them. Bookmarks allow you to tag these common nodes so that they are always accessible for connecting via the _Right Click_ plug context menu.

To add a node to the bookmarks :

- _Right Click_ on the node in the NodeGraph
- Check the _Bookmarked_ menu item

![Bookmark creation menu](images/bookmarkCreation.png)

Now you can connect that node to a compatible plug at any time :

- _Right Click_ on the plug to connect to in the NodeGraph
- Choose a bookmark from the menu

![Bookmark connection menu](images/bookmarkConnection.png)

> Tip : Bookmarks can also be accessed from the _Right Click_ code insertion menu on the Expression node.

"Teleporting"
-------------

Although bookmarks make it easier to _create_ long connections to distant nodes, they don't help with the clutter that long connections can produce in the node graph, with many connections criss-crossing underneath everything.

![Before teleporting](images/teleportingBefore.png)

One solution to this is to create a node to invisibly "teleport" the connection closer to its destination. Gaffer doesn't provide a special type of node for this, but instead simply allows you to hide the input connections for _any_ particular node. It's common to use a Dot for this purpose, starting by inserting it close to the destination nodes :

![Teleporting dot](images/teleportingDot.png)

The input can then be hidden as follows :

- _Right Click_ on the Dot node in the NodeGraph
- Deselect the _Show Input Connections_ menu item

![Teleporting dot](images/teleportingMenu.png)

The input connection to the Dot is drawn as a small stub. Hovering over it will reveal a tooltip containing the name of the connection source, and selecting the Dot will draw the full connection :

![Teleporting selection](images/teleportingSelection.png)

Backdrops
---------

USE THE FINAL GRAPH FROM THE GETTING STARTED TUTORIAL

LIST THE PROPERTIES OF A BACKDROP

Boxes
-----

MENTION REFERENCING AND THE UIEDITOR AS A LITTLE BIT OF A TEASER FOR ANOTHER TUTORIAL?
