Multi-architecture development coding guideline
###############################################

This document is intended for guiding initial development of
ACRN multi-architecture.

This document is NOT a coding style guideline (where one normally expects
that the document explains things such as indentation, naming convention,
typedef rules, etc.)

During early development of multi-architecture project, the APIs
and data structures should be organized in a consistent approach for
better readability and maintainability. There are multiple ways to organize
these factors and this documentation lays out a guideline to assist future
development.

General Assumptions and Practices
*********************************

Due to FuSa constraints, we have several compiler features or coding styles that
are NOT encouraged in ``hypervisor/`` code:

* Weak link (``__weak``): There are several MISRA rules that discourage uses of weak
  link (Rule 1.2, 5.3, 5.8, 5.9). See `coding-guidelines`_ for more information.

* Function-like macro: Use inline function whenever possible. Function-like macros
  should be avoided unless absolutely necessary. This also implies that section-based
  variable placement is also discouraged (utilizes linker to group symbols in certain
  binary sections. See ``devicemodel/include/types.h``, macro ``SET_*``).

Practices and general rule of thumb:

* Enable compiler ``-O2`` as default.

* APIs that are public (globally visible) but implemented in arch domains should start
  their name with ``arch_``. If this API is optional, a default/empty ``arch_`` API
  can be implemented in common C/header file.

* Architecture headers should be exposure-controlled: If a structure or an API is used/shared
  only within the module, then do not expose it to public headers, put it to private headers.
  There is also a reverse statement: all arch symbols exposed to common scope should be public.
  This is nearly impossible in practice. So we follow only the former part, and leave the latter
  as future optimization.

.. _coding-guidelines: https://projectacrn.github.io/latest/developer-guides/coding_guidelines.html

Data Structures
***************

In multi-architecture project we might have a common data structure that needs
to carry some architecture specific information (for example, a vCPU object).

For this type of data structures we prefer the following style:

In common header:

.. code-block:: c

   /* File: common.h */
   #include <arch-public.h>

   struct foo {
       <common member1>;
       <common member2>;
       <common member3>;
       struct arch_foo arch;
   };

   struct bar {
       <common member1>;
       <common member2>;
       <common member3>;
   }

And in architecture specific header:

.. code-block:: c

   /* File: arch-public.h */

   /* do NOT include common.h here */

   struct arch_foo {
       <arch member1>;
       <arch member2>;
       <arch member3>;
   };

   /* Forward declaration when the reference of struct is opaque */
   struct foo;
   struct bar;

   struct baz {
       struct foo *f;
       struct bar *b;
   }

   /* Forward declaration is also needed for function argument pointer */
   void some_function(struct foo *);

The architecture public header should NOT in turn include common header
that already included the public header.

Consider an inclusion graph with nodes being headers, and edges being
inclusions. If A includes B, there's an edge from A to B. Ideally, the
header inclusion graph should be **acyclic**.

If a downstream header needs something from upstream headers, (upstream and
downstream refers to positions relative to inclusion chain) a forward declaration
SHOULD be sufficient. If not, then there are problems with your organization of headers.

However in practice, people will not spend time following inclusion chain to
check if it is acyclic, and this issue is often noticable when there's a circular
dependency compilation error. In most projects (especially large ones), as long
as it compiles, it is OK.

So in our project, the **acyclic inclusion** is not an enforcement, but a
practice that is strongly advised.

Interfaces
**********

Here we consider only common-arch interfaces. Interfaces within each architecture
are out of scope of this section.

There are several form of interfaces used in different use cases.

Function prototypes
===================

This is the most common form of interface where a function is referenced in common scope
and implemented in arch-specific scope.

We prefer the following style:

In common header:

.. code-block:: c

   /* File: common.h */

   #include <arch-public.h>

   /*
    * Each architecture MUST implement this API in arch C file.
    * The implementation SHOULD NOT be marked as static inline
    * even if the implementation is small.
    */
   void arch_api_mandatory(void);

   /*
    * Short helpers that each architecture MAY provide.
    * Due to FuSa constraints described above,
    * Use a static inline function instead of a macro
    * to select arch implementation.
    */
   static inline void quick_helper(void) { arch_helper(); }

   /*
    * For short helpers from each architecture that SHOULD be
    * implemented as static inline function (or macro),
    * It is also OK to implement them directly in arch-public
    * header.
    */

   /*
    * Architecture MAY implement this API.
    * If it does not, an empty stub is provided.
    * Define ARCH_HAS_SOME_CAPABILITY in arch headers to
    * indicate implementation.
    */
   #ifdef ARCH_HAS_SOME_CAPABILITY
   int arch_api_optional(void);
   #else
   static inline int arch_api_optional(void) { return 0; }
   #endif

   /*
    * Architecture MAY implement this API.
    * If it does not, a common default is provided in common C file.
    * Due to FuSa constraint described above, __weak is discouraged.
    * Define ARCH_HAS_OPTIONAL_WITH_DEFAULT in arch header to
    * indicate implementation instead.
    */
   void arch_api_optional_with_default(void);

   /*
    * Configuration-based APIs
    * The CONFIG macro is provided by header generated by build-system.
    */
   #ifdef CONFIG_SWITCH_1
   void arch_api_conditional(void);
   #endif

In common C file:

.. code-block:: c

   /* File: common.c */

   #include <common.h>

   #ifndef ARCH_HAS_OPTIONAL_WITH_DEFAULT
   void arch_api_optional_with_default(void) {
       /* default implementation */
   }
   #endif

   void common_logic(void) {
       arch_api_mandatory();

       arch_api_optional();

       arch_api_optional_with_default();

   #ifdef CONFIG_SWITCH_1
       arch_api_conditional();
   #endif
   }

In architecture-specific public header file:

.. code-block:: c

   /* File: arch-public.h */

   /* Indicates implementation to optional API */
   #define ARCH_HAS_SOME_CAPABILITY

   /* Indicates implementation to optional_with_default API */
   #define ARCH_HAS_OPTIONAL_WITH_DEFAULT

   static inline void arch_helper(void) {
       /* short implementation */
   }

In architecture-specific C file:

.. code-block:: c

   /* File: arch.c */
   #include <common.h>
   #include <arch-priate.h>

   void arch_api_mandatory(void) {
       /* implementation */
   }

   /* ARCH_HAS_SOME_CAPABILITY is defined in arch-public header */
   void arch_api_optional(void) {
       /* implementation */
   }

   /* ARCH_HAS_OPTIONAL_WITH_DEFAULT is defined in arch-public header */
   void arch_api_optional_with_default(void) {
       /* implementation */
   }

   #ifdef CONFIG_SWITCH_1
   void arch_api_conditional(void) {
       /* implementation */
   }
   #endif

Register Callbacks
==================

Callbacks are used when we do NOT know which implementation to be used
until runtime. To determine implementation we need to do either detection,
or accepting user input.

With this approach, all implementations will be compiled-in, and each implementation
exposes a callback data structure typically called ``*_ops`` to common scope.

In common header file:

.. code-block:: c

   /* File: common.h */
   struct foo_ops {
       void (*op1)(void);
       int (*op2)(int);
       int (*op3)(struct bar *);
   };

   extern struct foo_ops a_ops;
   extern struct foo_ops b_ops;


In common C file:

.. code-block:: c

   /* File: common.c */

   #include <common.h>

   void do_detection() {
       /* detects environment */
       if (condition_a) {
           register_callback(a_ops);
       } else {
           register_callback(b_ops);
       }
   }

   void actual_call() {
       op->op1();
       op->op2(1);
   }

In arch header file:

.. code-block:: c

   /* File: arch-public.h */

   /* Do nothing */

.. code-block:: c

   /* File: arch.c */
   #include <common.h>

   static void a_op1(void) {
       /* implementation */
   }

   static int a_op2(void) {
       /* implementation */
   }

   static int a_op2(struct bar *b) {
       /* implementation */
   }

   const struct foo_ops a_ops = {
       .op1 = a_op1,
       .op2 = a_op2,
       .op3 = a_op3,
   }
