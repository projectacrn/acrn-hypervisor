.. _graphviz-examples:

Drawings Using Graphviz
#######################

We support using the Sphinx `Graphviz extension`_ for creating simple
graphs and line drawings using the dot language.  The advantage of using
Graphviz for drawings is that the source for a drawing is a text file that
can be edited and maintained in the repo along with the documentation.

.. _graphviz extension: http://graphviz.gitlab.io

These source ``.dot`` files are generally kept separate from the document
itself, and included by using a Graphviz directive:

.. code-block:: none

   .. graphviz:: images/boot-flow.dot
      :name: boot-flow-example
      :align: center
      :caption: ACRN Hypervisor Boot Flow

where the boot-flow.dot file contains the drawing commands:

.. literalinclude:: images/boot-flow.dot

and the generated output would appear as:

.. graphviz:: images/boot-flow.dot
  :name: boot-flow-example
  :align: center
  :caption: ACRN Hypervisor Boot Flow


Let's look at some more examples and then we'll get into more details about
the dot language and drawing options.

Simple Directed Graph
*********************

For simple drawings with shapes and lines, you can put the Graphviz commands
in the content block for the directive. For example, for a simple directed
graph (digraph) with two nodes connected by an arrow, you can write:


.. code-block:: none

    .. graphviz::

       digraph {
          "a" -> "b"
       }

and get this drawing:

.. graphviz::

   digraph {
      "a" -> "b"
   }


You can change the following attributes:

* Graph layout (from top-to-bottom to left-to-right)
* Node shapes (rectangles, circles, houses, stars, etc.)
* Style (filled, rounded)
* Colors
* Text displayed in the node
* Placement of the resulting image on the page (centered)

Example: 

.. literalinclude:: images/circle-square.dot

.. graphviz:: images/circle-square.dot
   :align: center

You can use the `standard HTML color names`_ or use RGB values for colors,
as shown.

.. _standard HTML color names:
   https://www.w3schools.com/colors/colors_hex.asp

Adding Edge Labels
******************

Here's an example of a drawing with labels on the edges (arrows) between
nodes. We also show how to change the default attributes for all nodes and
edges within this graph:

.. literalinclude:: images/node-shape-edges.dot

.. graphviz:: images/node-shape-edges.dot
   :align: center

Tables
******

For nodes with a ``record`` shape attribute, the text of the label is
presented in a table format:  a vertical bar ``|`` starts a new row or
column and curly braces ``{ ... }`` specify a new row (if you're in a
column) or a new column (if you're in a row).  For example:

.. literalinclude:: images/record.dot

.. graphviz:: images/record.dot
   :align: center

Note that you can also specify the horizontal alignment of text using escape
sequences ``\n``, ``\l``, and ``\r``, which divide the label into lines that
are centered, left-justified, and right-justified, respectively.

Finite-State Machine
********************

Here's an example of using Graphviz for defining a finite-state machine
for pumping gas:

.. literalinclude:: images/gaspump.dot

.. graphviz:: images/gaspump.dot
   :align: center
