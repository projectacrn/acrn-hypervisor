.. _acrn_home:

Project ACRN documentation
##########################

Welcome to the Project ACRN (version |version|) documentation.  ACRN is
a flexible, lightweight reference hypervisor, built with real-time and
safety-criticality in mind, optimized to streamline embedded development
through an open source platform. 

.. comment The links in this grid display can't use :ref: because we're
   using raw html.  There's a risk of broken links if referenced content is
   moved.

.. raw:: html

   <ul class="grid">
       <li class="grid-item">
           <a href="learn.html">
               <img alt="" src="_static/images/ACRNlogo.png"/>
               <h2>Learn about<br/>ACRN</h2>
           </a>
           <p>Overview, architecture,
              features, and use-case scenarios</p>
       </li>
       <li class="grid-item">
           <a href="try.html">
               <span class="grid-icon fa fa-map-signs"></span>
               <h2>Try using<br/>ACRN</h2>
           </a>
           <p>Getting started guides for quickly running scenario-based
              samples</p>
       </li>
       <li class="grid-item">
           <a href="develop.html">
               <span class="grid-icon fa fa-cogs"></span>
               <h2>Develop using<br/>ACRN</h2>
           </a>
           <p>Details for developing hypervisor-based
              solutions</p>
       </li>
       <li class="grid-item">
           <a href="contribute.html">
               <span class="grid-icon fa fa-github"></span>
               <h2>Contribute to<br/>ACRN</h2>
           </a>
           <p>High-level design and details, developer
              guidelines</p>
       </li>
       <li class="grid-item">
           <a href="reference/index.html">
               <span class="grid-icon fa fa-sign-in"></span>
               <h2>Developer<br/>Reference</h2>
           </a>
           <p>API details, configuration
              options, and site index</p>
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
   learn
   try
   develop
   contribute
   reference/index
   release_notes/index
   faq

.. _BSD 3-clause license:
   https://github.com/projectacrn/acrn-hypervisor/blob/master/LICENSE

.. _Project ACRN GitHub repo: https://github.com/projectacrn
