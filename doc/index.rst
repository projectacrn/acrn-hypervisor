.. _acrn_home:

Project ACRN Documentation
##########################

Welcome to the Project ACRN (version |version|) documentation published |today|.
ACRN is a flexible, lightweight reference hypervisor, built with real-time and
safety-criticality in mind, optimized to streamline embedded development through
an open source platform.

.. comment The links in this grid display can't use :ref: because we're
   using raw html.  There's a risk of broken links if referenced content is
   moved.

.. raw:: html

   <ul class="grid">
       <li class="grid-item">
           <a href="introduction/index.html">
               <img alt="" src="_static/images/ACRNlogo80w.png"/>
               <h2>What is ACRN</h2>
           </a>
           <p>Overview, architecture,
              features, and use-case scenarios</p>
       </li>
       <li class="grid-item">
           <a href="try.html">
               <span class="grid-icon fa fa-map-signs"></span>
               <h2>Getting Started</h2>
           </a>
           <p>Getting started guides for quickly running scenario-based
              samples</p>
       </li>
       <li class="grid-item">
           <a href="develop.html">
               <span class="grid-icon fa fa-cogs"></span>
               <h2>Advanced Guides</h2>
           </a>
           <p>Tools, tutorials, features, and debugging guides that go beyond getting started</p>
       </li>
       <li class="grid-item">
           <a href="contribute.html">
               <span class="grid-icon fa fa-github"></span>
               <h2>Developer Reference</h2>
           </a>
           <p>High-level design and details, developer and
              contribution guidelines, API details</p>
       </li>
       <li class="grid-item">
           <a href="release_notes/index.html">
               <span class="grid-icon fa fa-sign-in"></span>
               <h2>Release<br/>Notes</h2>
           </a>
           <p>Archived Release Notes</p>
       </li>
       <li class="grid-item">
           <a href="reference/hardware.html">
               <span class="grid-icon fa fa-object-group"></span>
               <h2>Supported<br/>Hardware</h2>
           </a>
           <p>Supported hardware platforms and boards</p>
       </li>
   </ul>


Source code for Project ACRN is maintained in the
`Project ACRN GitHub repo`_, and is provided under the BSD 3-clause
license.

.. toctree::
   :maxdepth: 1
   :hidden:

   Documentation Home <self>
   introduction/index
   try
   develop
   contribute
   release_notes/index
   asa
   projects/index
   glossary
   genindex

.. _Project ACRN GitHub repo: https://github.com/projectacrn
