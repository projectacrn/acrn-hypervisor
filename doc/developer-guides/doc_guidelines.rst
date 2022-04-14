.. _doc_guidelines:

Documentation Guidelines
########################

Project ACRN content is written using the `reStructuredText`_ markup
language (``.rst`` file extension) with Sphinx extensions, and processed
using Sphinx to create a formatted stand-alone website.  Developers can
view this content either in its raw form as ``.rst`` markup files, or (with
Sphinx installed) they can build the documentation using the Makefile
(on Linux systems) to generate the HTML content. The HTML content can then be
viewed using a web browser. This same ``.rst`` content is also fed into the
`Project ACRN documentation`_ website.

You can read details about `reStructuredText`_ and about `Sphinx extensions`_
from their respective websites.

.. _Sphinx extensions: https://www.sphinx-doc.org/en/stable/contents.html
.. _reStructuredText: http://docutils.sourceforge.net/docs/ref/rst/restructuredtext.html
.. _Sphinx Inline Markup: https://www.sphinx-doc.org/en/master/usage/restructuredtext/roles.html
.. _Project ACRN documentation:  https://projectacrn.github.io

This document provides a quick reference for commonly used reST and
Sphinx-defined directives and roles used to create the documentation
you're reading.

Headings
********

Document sections are identified through their heading titles, indicated with
an underline below the title text.  (While reST allows use of both and
overline and matching underline to indicate a heading, we use only an
underline indicator for headings.)  For consistency in our documentation, we
define the order of characters used to indicate the nested levels in the
table of contents:

* Use ``#`` for the Document title underline character
* Use ``*`` for the First sub-section heading level
* Use ``=`` for the Second sub-section heading level
* Use ``-`` for the Third sub-section heading level

Additional heading-level depth is discouraged.

The heading underline must be at least as long as the title it's under.

Here's an example of nested heading levels and the appropriate
underlines to use:

.. code-block:: rest

   Document Title heading
   ######################

   Section 1 heading
   *****************

   Section 2 heading
   *****************

   Section 2.1 heading
   ===================

   Section 2.1.1 heading
   ---------------------

   Section 2.2 heading
   ===================

   Section 3 heading
   *****************



Content Highlighting
********************

Some common reST inline markup samples:

* one asterisk: ``*text*`` for emphasis (*italics*),
* two asterisks: ``**text**`` for strong emphasis (**boldface**), and
* two back quotes: ````text```` for ``inline code`` samples.

ReST rules for inline markup try to be forgiving to account for common
cases of using these marks.  For example, using an asterisk to indicate
multiplication, such as ``2 * (x + y)`` will not be interpreted as an
unterminated italics section. For inline markup, the characters between
the beginning and ending characters must not start or end with a space,
so ``*this is italics*`` ( *this is italics*) while ``* this isn't*``
(* this isn't*).

If an asterisk or back quote appears in running text and could be confused
with inline markup delimiters, you can eliminate the confusion by adding a
backslash (``\``) before it.

Lists
*****

For bullet lists, place an asterisk (``*``) or hyphen (``-``) at the start of
a paragraph and indent continuation lines with two spaces.

The first item in a list (or sublist) must have a blank line before it and
should be indented at the same level as the preceding paragraph (and not
indented itself).

For numbered lists
start with a ``1.`` or ``a)`` for example, and continue with autonumbering by
using a ``#`` sign and a ``.`` or ``)`` as used in the first list item.
Indent continuation lines with spaces to align with the text of first
list item:

.. code-block:: rest

   * This is a bulleted list.
   * It has two items, the second
     item and has more than one line of reST text.  Additional lines
     are indented to the first character of the
     text of the bullet list.

   1. This is a new numbered list. If there wasn't a blank line before it,
      it would be a continuation of the previous list (or paragraph).
   #. It has two items too.

   a) This is a numbered list using alphabetic list headings
   #) It has three items (and uses autonumbering for the rest of the list)
   #) Here's the third item.  Use consistent punctuation on the list
      number.

   #. This is an autonumbered list (default is to use numbers starting
      with 1).

      #. This is a second-level list under the first item (also
         autonumbered).  Notice the indenting.
      #. And a second item in the nested list.
   #. And a second item back in the containing list.  No blank line
      needed, but it wouldn't hurt for readability.

Definition lists (with one or more terms and their definition) are a
convenient way to document a word or phrase with an explanation.  For example,
this reST content:

.. code-block:: rest

   The Makefile has targets that include:

   ``html``
      Build the HTML output for the project

   ``clean``
      Remove all generated output, restoring the folders to a
      clean state.

Would be rendered as:

   The Makefile has targets that include:

   html
      Build the HTML output for the project

   clean
      Remove all generated output, restoring the folders to a
      clean state.

Multi-Column Lists
******************

If you have a long bullet list of items, where each item is short, you can
indicate that the list items should be rendered in multiple columns with a
special ``.. rst-class:: rst-columns`` directive.  The directive will apply to
the next non-comment element (for example, paragraph) or to content indented under
the directive. For example, this unordered list::

   .. rst-class:: rst-columns

   * A list of
   * short items
   * that should be
   * displayed
   * horizontally
   * so it doesn't
   * use up so much
   * space on
   * the page

would be rendered as:

.. rst-class:: rst-columns

   * A list of
   * short items
   * that should be
   * displayed
   * horizontally
   * so it doesn't
   * use up so much
   * space on
   * the page

A maximum of three columns will be displayed if you use ``rst-columns``
(or ``rst-columns3``), and two columns for ``rst-columns2``. The number
of columns displayed can be reduced based on the available width of the
display window, reducing to one column on narrow (phone) screens if necessary.

.. note:: We've deprecated use of the ``hlist`` directive because it
   misbehaves on smaller screens.

Tables
******

There are a few ways to create tables, each with their limitations or quirks.
`Grid tables
<http://docutils.sourceforge.net/docs/ref/rst/restructuredtext.html#grid-tables>`_
offer the most capability for defining merged rows and columns, but are
hard to maintain::

   +------------------------+------------+----------+----------+
   | Header row, column 1   | Header 2   | Header 3 | Header 4 |
   | (header rows optional) |            |          |          |
   +========================+============+==========+==========+
   | body row 1, column 1   | column 2   | column 3 | column 4 |
   +------------------------+------------+----------+----------+
   | body row 2             | ...        | ...      | you can  |
   +------------------------+------------+----------+ easily   +
   | body row 3 with a two column span   | ...      | span     |
   +------------------------+------------+----------+ rows     +
   | body row 4             | ...        | ...      | too      |
   +------------------------+------------+----------+----------+

This example would render as:

+------------------------+------------+----------+----------+
| Header row, column 1   | Header 2   | Header 3 | Header 4 |
| (header rows optional) |            |          |          |
+========================+============+==========+==========+
| body row 1, column 1   | column 2   | column 3 | column 4 |
+------------------------+------------+----------+----------+
| body row 2             | ...        | ...      | you can  |
+------------------------+------------+----------+ easily   +
| body row 3 with a two column span   | ...      | span     |
+------------------------+------------+----------+ rows     +
| body row 4             | ...        | ...      | too      |
+------------------------+------------+----------+----------+

`List tables
<http://docutils.sourceforge.net/docs/ref/rst/directives.html#list-table>`_
are much easier to maintain, but don't support row or column spans::

   .. list-table:: Table title
      :widths: 15 20 40
      :header-rows: 1

      * - Heading 1
        - Heading 2
        - Heading 3
      * - body row 1, column 1
        - body row 1, column 2
        - body row 1, column 3
      * - body row 2, column 1
        - body row 2, column 2
        - body row 2, column 3

This example would render as:

.. list-table:: Table title
   :widths: 15 20 40
   :header-rows: 1

   * - Heading 1
     - Heading 2
     - Heading 3
   * - body row 1, column 1
     - body row 1, column 2
     - body row 1, column 3
   * - body row 2, column 1
     - body row 2, column 2
     - body row 2, column 3

The ``:widths:`` parameter lets you define relative column widths.  The
default is equal column widths. If you have a three-column table and you
want the first column to be half as wide as the other two equal-width
columns, you can specify ``:widths: 1 2 2``.  If you'd like the browser
to set the column widths automatically based on the column contents, you
can use ``:widths: auto``.

File Names and Commands
***********************

Sphinx extends reST by supporting additional inline markup elements (called
"roles") used to tag text with special meanings and enable output formatting.
(You can refer to the `Sphinx Inline Markup`_ documentation for the full
list).

For example, there are roles for marking :file:`filenames`
(``:file:`name```) and command names such as :command:`make`
(``:command:`make```).  You can also use the \`\`inline code\`\`
markup (double backticks) to indicate a ``filename``.

Don't use items within a single backtick, for example ```word```. Instead
use double backticks: ````word````.

Branch-Specific File Links
**************************

You can add a link in the documentation to a specific file in the GitHub tree.
Be sure the link points to the branch for that version of the documentation. For
example, links in the v2.5 release of the documentation should be to files in
the v2.5 branch. Do not link to files in the master branch because files in that
branch could change or even be deleted after the release is made.

To make this kind of file linking possible, use a special role that
creates a hyperlink to that file in the current branch.  For example, a GitHub
link to the reST file used to create this document can be generated
using ``:acrn_file:`doc/developer-guides/doc_guidelines.rst```, which will
appear as :acrn_file:`doc/developer-guides/doc_guidelines.rst`, a link to
the "blob" file in the GitHub repo as displayed by GitHub. There's also an
``:acrn_raw:`doc/developer-guides/doc_guidelines.rst``` role that will link
to the "raw" uninterpreted file,
:acrn_raw:`doc/developer-guides/doc_guidelines.rst`. Click these links
to see the difference.

If you don't want the whole path to the file name to
appear in the text, you use the usual linking notation to define what link text
is shown, for example, ``:acrn_file:`Guidelines <doc/developer-guides/doc_guidelines.rst>```
would show up as simply :acrn_file:`Guidelines <doc/developer-guides/doc_guidelines.rst>`.

.. _internal-linking:

Internal Cross-Reference Linking
********************************

Traditional ReST links are supported only within the current file using the
notation:

.. code-block:: rest

   refer to the `internal-linking`_ page

which renders as,

   refer to the `internal-linking`_ page

Note the use of a trailing underscore to indicate an outbound link. In this
example, the label was added immediately before a heading, so the text that's
displayed is the heading text itself.

With Sphinx, however, we can create link-references to any tagged text within
the project documentation.

Target locations within documents are defined with a label directive:

   .. code-block:: rst

      .. _my label name:

Note the leading underscore indicating an inbound link. The content
immediately following this label is the target for a ``:ref:`my label name```
reference from anywhere within the documentation set. The label **must** be
added immediately before a heading so that there's a natural phrase to show
when referencing this label (for example, the heading text).

This directive is also used to define a label that's a reference to a URL:

.. code-block:: rest

   .. _Hypervisor Wikipedia Page:
      https://en.wikipedia.org/wiki/Hypervisor

To enable easy cross-page linking within the site, each file should have a
reference label before its title so that it can be referenced from another
file.

.. note:: These reference labels must be unique across the whole site, so generic
   names such as "samples" should be avoided.

For example, the top of this
document's ``.rst`` file is:

.. code-block:: rst

   .. _doc_guidelines:

   Documentation Guidelines
   ########################

Other ``.rst`` documents can link to this document using the
``:ref:`doc_guidelines``` tag, and it will appear as :ref:`doc_guidelines`.
This type of internal cross-reference works across multiple files. The link
text is obtained from the document source, so if the title changes, the link
text will automatically update as well.

There may be times when you'd like to change the link text that's shown in the
generated document.  In this case, you can specify alternate text using
``:ref:`alternate text <doc_guidelines>``` (renders as
:ref:`alternate text <doc_guidelines>`).


Non-ASCII Characters
********************

You can insert non-ASCII characters such as a Trademark symbol (|trade|) by
using the notation ``|trade|``.  (It's also allowed to use the UTF-8
characters directly.) Available replacement names are defined in an include
file used during the Sphinx processing of the reST files.  The names of these
replacement characters are the same as those used in HTML entities to insert
special characters such as \&trade; and are defined in the file
``sphinx_build/substitutions.txt`` as listed here:

.. literalinclude:: ../substitutions.txt
   :language: rst

We've kept the substitutions list small but you can add others as needed by
submitting a change to the ``substitutions.txt`` file.

Code and Command Examples
*************************

Use the reST ``code-block`` directive to create a highlighted block of
fixed-width text, typically used for showing formatted code or console
commands and output.  Smart syntax highlighting is also supported (using the
Pygments package). You can also directly specify the highlighting language.
For example:

.. code-block:: rest

   .. code-block:: c

      struct _k_object {
         char *name;
         u8_t perms[CONFIG_MAX_THREAD_BYTES];
         u8_t type;
         u8_t flags;
         u32_t data;
      } __packed;

Note that there is a blank line between the ``code-block`` directive and the
first line of the code-block body, and the body content is indented three
spaces (to the first non-blank space of the directive name).

This example would render as:

.. code-block:: c

  struct _k_object {
     char *name;
     u8_t perms[CONFIG_MAX_THREAD_BYTES];
     u8_t type;
     u8_t flags;
     u32_t data;
  } __packed;


You can specify other languages for the ``code-block`` directive, including
``c``, ``python``, and ``rst``, and also ``console``, ``bash``, or ``shell``.
If you want no syntax highlighting, specify ``none``. For example:

.. code-block:: rest

   .. code-block:: none

      This block of text would be styled with a background
      and box, but with no syntax highlighting.

Would display as:

.. code-block:: none

  This block of text would be styled with a background
  and box, but with no syntax highlighting.

There's a shorthand for writing code blocks, too: end the introductory
paragraph with a double colon (``::``) and indent the code block content
by three spaces.  On output, only one colon will appear.

.. note:: The highlighting package makes a best guess at the type of content
   in the block, which can lead to odd
   highlighting in the generated output.

Images
******

Images are included in documentation by using an image directive::

   .. image:: ../../images/doc-gen-flow.png
      :align: center
      :alt: alt text for the image

or if you'd like to add an image caption, use::

    .. figure:: ../../images/doc-gen-flow.png
       :alt: image description

       Caption for the figure

The file name specified is relative to the document source file, and we
recommend putting images into an ``images`` folder where the document source
is found.  The usual image formats handled by a web browser are supported:
JPEG, PNG, GIF, and SVG.  Keep the image size only as large as needed,
generally at least 500 px wide but no more than 1000 px, and no more than
250 KB unless a particularly large image is needed for clarity.

Tabs, Spaces, and Indenting
***************************

Indenting is significant in reST file content, and using spaces is preferred.
Extra indenting can (unintentionally) change the way content is rendered, too.
For lists and directives, indent the content text to the first non-blank space
in the preceding line.  For example:

.. code-block:: rest

   * List item that spans multiple lines of text
     showing where to indent the continuation line.

   1. And for numbered list items, the continuation
      line should align with the text of the line above.

   .. code-block::

      The text within a directive block should align with the
      first character of the directive name.

Keep the line length for documentation fewer than 80 characters to make it
easier for reviewing in GitHub. Long lines due to URL references are an
allowed exception.

Background Colors
*****************

We've defined some CSS styles for use as background colors for paragraphs.
These styles can be applied using the ``.. rst-class`` directive using one of
these style names.  You can also use the defined ``centered`` style to place the
text centered within the element, useful for centering text within a table cell
or column span:

.. rst-class:: bg-acrn-green centered

   \.\. rst-class:: bg-acrn-green centered

.. rst-class:: bg-acrn-lightgreen centered

   \.\. rst-class:: bg-acrn-lightgreen centered

.. rst-class:: bg-acrn-brown centered

   \.\. rst-class:: bg-acrn-brown centered

.. rst-class:: bg-acrn-lightbrown centered

   \.\. rst-class:: bg-acrn-lightbrown centered

.. rst-class:: bg-acrn-blue centered

   \.\. rst-class:: bg-acrn-blue centered

.. rst-class:: bg-acrn-red centered

   \.\. rst-class:: bg-acrn-red centered

.. rst-class:: bg-acrn-gradient centered

   \.\. rst-class:: bg-acrn-gradient centered

.. rst-class:: bg-lightyellow centered

   \.\. rst-class:: bg-lightyellow centered

.. rst-class:: bg-lightgreen centered

   \.\. rst-class:: bg-lightgreen centered

.. rst-class:: bg-lavender centered

   \.\. rst-class:: bg-lavender centered

.. rst-class:: bg-lightgrey centered

   \.\. rst-class:: bg-lightgrey centered

Drawings
********

You can include a picture (.jpg, .png, or .svg for example) by using the
``.. image`` directive::

   .. image:: ../images/ACRNlogo.png
      :align: center

This results in the image being placed in the document:

.. image:: ../images/ACRNlogo.png
   :align: center

The preferred alternative is to use the ``.. figure`` directive to include a picture with
a caption and automatic figure numbering for your image, (so that you can say
see :numref:`acrn-logo-figure`, by using the notation
``:numref:`acrn-logo-figure``` and specifying the name of figure)::

   .. figure:: ../images/ACRNlogo.png
      :align: center
      :name: acrn-logo-figure

      Caption for the figure

.. figure:: ../images/ACRNlogo.png
   :align: center
   :name: acrn-logo-figure

   Caption for the figure

All figures should have a figure caption.

We've also included the ``graphviz`` Sphinx extension to enable you to use a
text description language to render drawings.  For more information, see
:ref:`graphviz-examples`.

Alternative Tabbed Content
**************************

Instead of creating multiple documents with common material except for some
specific sections, you can write one document and provide alternative content
to the reader via a tabbed interface. When the reader clicks a tab, the
content for that tab is displayed. For example::

   .. tabs::

      .. tab:: Apples

         Apples are green, or sometimes red.

      .. tab:: Pears

         Pears are green.

      .. tab:: Oranges

         Oranges are orange.

will display as:

.. tabs::

   .. tab:: Apples

      Apples are green, or sometimes red.

   .. tab:: Pears

      Pears are green.

   .. tab:: Oranges

      Oranges are orange.

Tabs can also be grouped so that changing the current tab in one area
changes all tabs with the same name throughout the page.  For example:

.. tabs::

   .. group-tab:: Linux

      Linux Line 1

   .. group-tab:: macOS

      macOS Line 1

   .. group-tab:: Windows

      Windows Line 1

.. tabs::

   .. group-tab:: Linux

      Linux Line 2

   .. group-tab:: macOS

      macOS Line 2

   .. group-tab:: Windows

      Windows Line 2

In this latter case, we're using a ``.. group-tab::`` directive instead of
a ``.. tab::`` directive.  Under the hood, we're using the `sphinx-tabs
<https://github.com/djungelorm/sphinx-tabs>`_ extension that's included
in the ACRN (requirements.txt)  setup.  Within a tab, you can have most
any content *other than a heading* (code-blocks, ordered and unordered
lists, pictures, paragraphs, and such).

Instruction Steps
*****************

A numbered instruction steps style makes it easy to create tutorial guides
with clearly identified steps. Add the ``.. rst-class:: numbered-step``
directive immediately before a second-level heading (by project convention, a
heading underlined with asterisks ``******``, and it will be displayed as a
numbered step, sequentially numbered within the document.  (Second-level
headings without this ``rst-class`` directive will not be numbered.)
For example::

   .. rst-class:: numbered-step

   Put your right hand in
   **********************

.. rst-class:: numbered-step

First Instruction Step
**********************

This is the first instruction step material.  You can do the usual paragraph
and pictures as you'd use in normal document writing. Write the heading to be
a summary of what the step is (the step numbering is automated so you can move
steps around easily if needed).

.. rst-class:: numbered-step

Second Instruction Step
***********************

This is the second instruction step.

.. note:: As implemented,
   only one set of numbered steps is intended per document and the steps
   must be level 2 headings.

Configuration Option Documentation
**********************************

Most of the ACRN documentation is maintained in ``.rst`` files found in the
``doc/`` folder.  API documentation is maintained as Doxygen comments in the C
header files,
along with some prose documentation in ``.rst`` files. The ACRN configuration
option documentation is created based on details maintained in schema definition
files (``.xsd``) in the ``misc/config_tools/schema`` folder.  These schema
definition files are used by the ACRN Configurator tool to validate the XML
scenario
configuration files as well as to hold documentation about each option.  For
example:

.. code-block:: xml

    <xs:element name="BUILD_TYPE" type="BuildType" default="debug">
      <xs:annotation acrn:title="Build type" acrn:views="basic">
        <xs:documentation>Identify build type.  Debug mode enables debug shell, prints, and logs.  Release mode optimizes the ACRN binary for deployment and turns off all debug infrastructure.  These can only be changed at build time.</xs:documentation>
      </xs:annotation>
    </xs:element>

During the documentation ``make html`` processing, the documentation annotations
in the ``.xsd`` files are extracted and transformed into reStructuredText using
an XSLT transformation found in ``doc/scripts/configdoc.xsl``. The generated
option documentation is organized and formatted to make it easy to create links
to specific option descriptions using an ``:option:`` role, for example,
``:option:`hv.DEBUG_OPTIONS.BUILD_TYPE``` would link to
:option:`hv.DEBUG_OPTIONS.BUILD_TYPE`.

The transformed option documentation is
created in the ``_build/rst/reference/configdoc.txt`` file and included by
``doc/reference/config-options.rst`` to create the final published
:ref:`scenario-config-options` document.  You modify the option
descriptions by editing the documentation found in one of the ``.xsd`` files.


Documentation Generation
************************

For instructions on building the documentation, see :ref:`acrn_doc`.
