.. _module_design_guidelines:

Module Design Guidelines
########################


ACRN hypervisor is decomposed into a series of components,
some of which are further decomposed into several modules.
The goal of ACRN module design is to further specify the
files implementing a module, the macros and declarations
in each file and the internals of each declaration.


This section summarizes the guidelines for ACRN module design.
It shall originally have the form of doxygen-style comments embedded
in the implementation.


.. contents::
   :local:


MD-01: Legal entity shall be documented in every file
=====================================================

Legal entity shall be documented in a separate comments block at the start of
every file. The following information shall be included:

a) Copyright
b) License (using an `SPDX-License-Identifier <https://spdx.org/licenses/>`_)

Compliant example::

    /* Legal entity shall be placed at the start of the file. */
    -------------File Contents Start After This Line------------

    /*
     * Copyright (C) 2019-2022 Intel Corporation.
     *
     * SPDX-License-Identifier: BSD-3-Clause
     */

    /* Coding or implementation related comments start after the legal entity. */
    #include <types.h>


.. rst-class:: non-compliant-code

   Non-compliant example::

       /* Neither copyright nor license information is included in the file. */
       -------------------File Contents Start After This Line------------------

       /* Coding or implementation related comments start directly. */
       #include <types.h>



MD-02: Components and modules shall be documented
=================================================

A component or a module shall have a corresponding group defined in the code comments.

 * Component documentation
    The group definition for a component can be documented in any C source file
    or header file belongs to that component.
    The template is given as follows::

	/**
	 * @defgroup {group name} {group title}
	 * @brief {brief description}
	 *
	 * {Detailed description on the functionality, purpose and use cases of the component}
	 */

    1. The group name for the component shall be unique throughout the source tree.
       It is recommended to make it same as the component name.
    2. The group title can be an string, briefly describing the functionality
       provided by the component. For simplicity, it can be same as the component name if the
       name is straightforward.


    Take the component `lib` as an example::

	/**
	 * @defgroup lib lib
	 * @brief {brief description}
	 *
	 * {Detailed description on the functionality, purpose and use cases of the component}
	 */

 * Module documentation
    The group definition for a module can be documented in any C source file
    or header file belongs to that module.
    The template is given as follows::

	/**
	 * @defgroup {group name} {group title}
	 * @ingroup {component name}
	 * @brief {brief description}
	 *
	 * {Detailed description on the functionality, purpose and use cases of the module}
	 */

    1. The group name for the module shall be unique throughout the source tree.
       The naming convention is ``<component name>_<module name>``.
    2. The naming convention for the group title is ``<component name>.<module name>``.
    3. The ``@ingroup`` keyword shall be used to refer to the component where the module is in.

    Take the module `util` in the component `lib` as an example::

	/**
	 * @defgroup lib_util lib.util
	 * @ingroup lib
	 * @brief {brief description}
	 *
	 * {Detailed description on the functionality, purpose and use cases of the module}
	 */


.. rst-class:: non-compliant-code

   Non-compliant example::

	/* {description for module x}
	 *
	 * {Detailed descriptions}
	 */



MD-03: Files shall be documented
================================

Each file shall be documented in the form shown in the `Compliant example 1`
and `Compliant example 2`.

A file documentation block begins with the keyword ``@file``,
followed by a brief description (with the keyword ``@brief``).
More detailed description of the current file follows at the end of the block.

Each file shall always be put under the scope of exactly one group.
It can be specified by using the opening and closing markers
(i.e. ``@{`` and ``@}``) after the ``@defgroup`` keyword or
``@addtogroup`` keyword.


Compliant example 1 (with the ``@defgroup`` keyword)::

	/*
	 * Copyright (C) 2019-2024 Intel Corporation.
	 *
	 * SPDX-License-Identifier: BSD-3-Clause
	 */

	 #include <xxx.h>
	 #include <yyy.h>
	 #include "zzz.h"

	/**
	 * @defgroup lib_util lib.util
	 * @ingroup lib
	 * @brief {brief description}
	 *
	 * {Detailed description on the functionality, purpose and use cases of the module}
	 *
	 * @{
	 */

	/**
	 * @file
	 * @brief {brief description}
	 *
	 * {Detailed description on the functionality provided by the
	 * file, purpose of the file and use cases of the file}
	 */

	/* FILE CONTENTS */

	/**
	 * @}
	 */

.. note::
   In `Compliant example 1`, the file belongs to the group `lib_util`.
   The information is illustrated by using the opening and closing markers
   (i.e. ``@{`` and ``@}``) after the ``@defgroup`` keyword.


Compliant example 2 (with the ``@addtogroup`` keyword)::

	/*
	 * Copyright (C) 2019-2024 Intel Corporation.
	 *
	 * SPDX-License-Identifier: BSD-3-Clause
	 */

	 #include <xxx.h>
	 #include <yyy.h>
	 #include "zzz.h"

	/**
	 * @addtogroup lib_util
	 *
	 * @{
	 */

	/**
	 * @file
	 * @brief {brief description}
	 *
	 * {Detailed description on the functionality provided by the
	 * file, purpose of the file and use cases of the file}
	 */

	/* FILE CONTENTS */

	/**
	 * @}
	 */

.. note::
   In `Compliant example 2`, the file belongs to the group `lib_util`.
   The information is illustrated by using the opening and closing markers
   (i.e. ``@{`` and ``@}``) after the ``@addtogroup`` keyword.


.. rst-class:: non-compliant-code

   Non-compliant example::

	 #include <xxx.h>
	 #include <yyy.h>
	 #include "zzz.h"

	/* {description}
	 *
	 * {Detailed descriptions}
	 */



.. _md_functions:

MD-04: Functions and function-like macros shall be documented
=============================================================

A function or function-like macro shall be documented in the form shown in the
`Compliant example`.
The documentation block shall be placed right before the function definition
in the C source file or header file.

Following documentation items shall be included when applicable.
   1) (Mandatory) The brief description of the function shall be documented with the format
      ``@brief <brief description>``.

   2) (Optional) The informative description of the function is used to illustrate its purpose,
      when it is supposed to be used, etc.

   3) (Mandatory if it is an external API of a component or a module)
      Normal case behaviors and error case behaviors shall be documented following
      the informative description (if applicable).
      It may be omitted if this function is an internal function in a component or a module.

       * Normal case behaviors of an API are effects that a caller can expect after calling
         the API if no error condition is satisfied.

       * Error case behaviors of an API are effects of the API when any error condition
         is satisfied and the expected behavior of this API cannot be fulfilled.
         Error case behavior may be omitted if the API shall always be able to conduct
         its expected behavior.

   6) (Mandatory if the function parameter is not void)
      The description of the function parameter shall be documented with the
      format ``@param[direction] <parameter name> <parameter description>``.
      It may be omitted if the function parameter is void.

       * Direction of a parameter shall be ``in`` if this function shall never modify the parameter.
         If the parameter is a pointer to a data structure, having a direction of ``in`` means
         the function shall modify neither the data structure itself nor any memory location that
         can be addressed by fetching pointers recursively from the data structure.
         This is similar to the const qualifier in C, but requires that any pointer in the
         pointed data structure is const as well.

       * Direction of a parameter shall be ``out`` if this function shall modify the parameter and
         shall not read from the parameter. In C, arguments having a direction of ``out`` shall
         always be pointers. An function shall never read any memory location pointed to by an ``out``
         parameter.

       * Direction of a parameter shall be ``inout`` if neither of the above cases apply,
         indicating that the function is free to read or modify the parameter.

   7) (Mandatory) The description of the return value shall be documented
      with the format ``@return <description of the return value>``.
      A void-returning function shall be documented with ``@return None``.

   8) (Mandatory if the return value represents error conditions)
      A list of more detailed specifications of return values shall be documented with
      the format ``@retval <return value> <return value explanation>``.
      This list is mandatory if the function returns an integer representing error code.
      For functions returning pointers or integers that do not range in a small fixed range,
      this list may be omitted.

   9) (Mandatory) The pre-condition of the function shall be documented with the format
      ``@pre <pre-condition description>``. C expressions shall be used when possible.
      If there is no pre-conditions, say ``@pre N/A``.

   10) (Mandatory) The post-condition of the function shall be documented with the format
       ``@post <post-condition description>``. C expressions shall be used when possible.
       If there is no post-conditions, say ``@post N/A``.

   11) (Optional) A set of remarks that specify the additional constraints on using this function
       shall be documented with the format ``@remark <additional constraints>``.


Compliant example::

	/**
	 * @brief {brief description}
	 *
	 * Informative description of this function, illustrating its purpose, when it is supposed to be used, etc.
	 * Rationales follow here if necessary.
	 *
	 * Normal case behaviors and error case behaviors follow here if this function is an external API of
	 * a component or a module.
	 * - Normal case behaviors of an API are effects that a caller can expect after calling the API if no error condition
	 *   is satisfied.
	 * - Error case behaviors of an API are effects of the API when any error condition is satisfied and the expected
	 *   behavior of this API cannot be fulfilled. Error case behavior may be omitted if the API shall always be able to
	 *   conduct its expected behavior.
	 *
	 * @param[inout] param_1 {param_1 description}
	 * @param[in] param_2 {param_2 description}
	 * @param[out] param_3 {Parameter description for param_3. Parameter description for param_3.
	 *                     Parameter description for param_3. Parameter description for param_3.
	 *                     Parameter description for param_3. Parameter description for param_3.}
	 *
	 * @return {what the return value represents}
	 *
	 * @retval -EIO {description when this error can happen}
	 * @retval -EIO {multiple cases can be splitted}
	 * @retval -EINVAL {description when this error can happen}
	 * @retval 0 Otherwise.
	 *
	 * @pre param_1 != NULL
	 * @pre param_2 != 0U
	 *
	 * @post retval <= 0
	 *
	 * @remark The API must be invoked with interrupt disabled.
	 * @remark {Other usage constraints here}
	 */
	int32_t func_showcase(uint32_t *param_1, uint32_t param_2, uint32_t* param_3);


.. rst-class:: non-compliant-code

   Non-compliant example::

       /* Brief description of the function.
       Detailed description of the function. Detailed description of the function. Detailed description of the
       function. Detailed description of the function.

       @param param_1 Parameter description for param_1. @param param_2 Parameter description for param_2.
       @param param_3 Parameter description for param_3. Parameter description for param_3. Parameter description
       for param_3. Parameter description for param_3. Parameter description for param_3. Parameter
       description for param_3.

       pre-conditions: param_1 != NULL, param_2 <= 255U
       post-conditions: retval <= 0

       Brief description of the return value. */
       int32_t func_showcase(uint32_t *param_1, uint32_t param_2, uint32_t param_3);



MD-05: Object-like macros shall be documented
=============================================

An object-like macro shall be documented in the form shown in the `Compliant example`.

It is recommended to put the description of the macro after the macro definition
for readability, as long as the description fits into the same line as the definition
(given the 120 character limit).
For object-like macros that require multiple lines to specify,
the comment shall be put before the macro.

Compliant example::

	#define MACRO_1 0x1000UL  /**< {description} */
	#define MACRO_2 0x2000UL  /**< {description} */
	#define MACRO_3 0x4000U   /**< {description} */

	/**
	 * @brief {Brief description}
	 *
	 * {Detailed descriptions}
	 */
	#define MACRO_1 0x1000UL


.. rst-class:: non-compliant-code

   Non-compliant example::

	#define MACRO_1 0x1000UL  // {description}
	#define MACRO_2 0x2000UL  /* {description} */

	/* {description} */
	#define MACRO_3 0x4000U

	/* {description}
	 *
	 * {Detailed descriptions}
	 */
	#define MACRO_1 0x1000UL



MD-06: Data structures shall be documented
==========================================

A struct or union definition shall be documented along with its members,
in the form shown in the `Compliant example`.

 * If the ``aligned`` attribute is used to specify the alignment (in bytes) of
   the data structure, it shall be documented with the ``@alignment`` keyword.
 * If the ``packed`` attribute is used to specify that each member
   (other than zero-width bit-fields) of the structure or union is placed to
   minimize the memory required, it shall be documented with
   ``@remark This structure shall be packed.``.

Compliant example::

	/**
	 * @brief {What this structure represents}
	 *
	 * {More details on what are represented and when it is
	 * supposed to be used}
	 *
	 * @consistency {consistency rule, e.g. self.a->b == &self}
	 * @alignment 8
	 *
	 * @remark {Constraints on uses, e.g. locks to acquire before accessing any fields}
	 * @remark This structure shall be packed.
	 */
	struct mmio_request {
	        uint32_t direction;  /**< Direction of this request. */
	        uint32_t reserved; /**< Reserved. */
	        int64_t address; /**< GPA of the register to be
	                          *   accessed. */

	        /**
	         * @brief Width of the register to be accessed.
	         *
	         * More multiple-paragraph descriptions.
	         */
	        int64_t size;

	        int64_t value; /**< Single-line comments can be appended
	                        *   after the field. */
	} __aligned(8) __packed;


.. rst-class:: non-compliant-code

   Non-compliant example::

	/* {description}
	 *
	 * {Detailed descriptions}
	 */
	struct mmio_request {
	        uint32_t direction;  // {description}
	        uint32_t reserved;  /* {description} */
	        int64_t address;   /* {description} */

	        /* {Brief description}
		 *
		 * {Detailed descriptions}
		 */
	        int64_t size;

	        int64_t value; // {description}
	} __aligned(8);



MD-07: Enumeration types shall be documented
============================================

An enumeration type shall be documented along with its constants,
in the form shown in the `Compliant example`.


Compliant example::

	/**
	 * @brief {what this enumeration type represents}
	 *
	 * {more details on the numeration type and when it is
	 * designed to be used}
	 */
	enum test_enum {
	        CONST_1 = 1,      /**< {what CONST_1 represents}.
	                           *   The value is fixed to 1. */
	        CONST_2,          /**< {what CONST_2 represents}. */
	        CONST_3,          /**< {what CONST_3 represents}. */
	        CONST_100 = 100,  /**< {what CONST_100 represents}.
	                           *   The value is fixed to 100. */
	};


.. rst-class:: non-compliant-code

   Non-compliant example::

	/* {description}
	 *
	 * {Detailed descriptions}
	 */
	enum test_enum {
	        CONST_1 = 1,      /* {what CONST_1 represents}.
	                           *   The value is fixed to 1. */
	        CONST_2,          /* {what CONST_2 represents}. */
	        CONST_3,          /* {what CONST_3 represents}. */
	        CONST_100 = 100,  /* {what CONST_100 represents}.
	                           *   The value is fixed to 100. */
	};



MD-08: Typedefs shall be documented
===================================

A typedef shall be documented in the form shown in the `Compliant example`,
mainly including the brief and detailed description which elaborates
what it represents and when it is designed to be used.

Compliant example::

	/**
	 * @brief {what this type represents}
	 *
	 * {more details on the represented type and when it is
	 * designed to be used}
	 */
	typedef int(*fn)(int i, int j);


.. rst-class:: non-compliant-code

   Non-compliant example::

	/* {description}
	 *
	 * {Detailed descriptions}
	 */
	typedef int(*fn)(int i, int j);



.. _md_global_variables:

MD-09: Global variables shall be documented
===========================================

A global variable shall be documented in the form shown in the `Compliant example`.

Compliant example::

	/**
	 * @brief {Brief description}
	 *
	 * {Detailed descriptions}
	 */
	static spinlock_t cmos_lock;


.. rst-class:: non-compliant-code

   Non-compliant example::

	/* {description}
	 *
	 * {Detailed descriptions}
	 */
	static spinlock_t cmos_lock;



MD-10: Assembly labels shall be documented
==========================================

The documentation block for assembly labels shall be placed right before
its declaration in the C header file.
Assembly labels defined in assembly files are either pointing to code or
pointing to data.

 * Labels pointing to code are modeled as function elements and shall be
   documented like functions, refer to the
   :ref:`guidelines for functions <md_functions>`.
 * Labels pointing to data are modeled as variables and shall be documented
   like global variables, refer to the
   :ref:`guidelines for global variables <md_global_variables>`.

