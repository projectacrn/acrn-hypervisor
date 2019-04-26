.. _c_coding_guidelines:

C Programming Language Coding Guidelines
########################################

.. contents::
   :local:


Preprocessor
************

C-PP-01: ## or # operators shall be used with restrictions
==========================================================

## or # operators shall only be used alone. The following cases shall not be
allowed:

a) The result getting from ## or # operation shall not be used as the operands
   of another ## or # operation;
b) Mixed-use of ## or # operators shall not be allowed.

Compliant example::

    #define CONCAT(x, y) x ## y
    
    uint32_t ab = 32U;
    printf("%d \n", CONCAT(a, b));

.. rst-class:: non-compliant-code

   Non-compliant example::

       #define CONCAT(x, y, z) x ## y ## z
       
       uint32_t abc = 32U;
       printf("%d \n", CONCAT(a, b, c));


C-PP-02: Function-like MACRO shall be used with restrictions
============================================================

Function-like MACRO shall be replaced with inline function if it is possible.

Compliant example::

    static inline uint32_t func_showcase(uint32_t a, uint32_t b)
    {
            return a + b;
    }

.. rst-class:: non-compliant-code

   Non-compliant example::

       #define SHOWCASE(a, b) ((a) + (b))


C-PP-03: Header file shall not be included multiple times
=========================================================

The content inside shall be protected with #ifndef, #if !defined, or #ifdef.

Compliant example::

    /* In `showcase.h`: */
    #ifndef SHOWCASE_H
    #define SHOWCASE_H
    
    /* header contents */
    uint32_t func_showcase(uint32_t param);
    
    #endif /* SHOWCASE_H */

.. rst-class:: non-compliant-code

   Non-compliant example::

       /* In `showcase.h`: */
       
       /* header contents without any protection */
       uint32_t func_showcase(uint32_t param);


C-PP-04: Parentheses shall be used when referencing a MACRO parameter
=====================================================================

Compliant example::

    #define NEGATING(x) -(x)

.. rst-class:: non-compliant-code

   Non-compliant example::

       #define NEGATING(x) -x



Compilation Units
*****************

C-CU-01: Only one assignment shall be on a single line
======================================================

Multiple assignments on a single line are not allowed.

Compliant example::

    a = d;
    b = d;
    c = d;

.. rst-class:: non-compliant-code

   Non-compliant example::

       int a = b = c = d;


C-CU-02: Only one return statement shall be in a function
=========================================================

Multiple return statements in a function are not allowed.

Compliant example::

    int32_t foo(char *ptr)
    {
            int32_t ret;
    
            if (ptr == NULL) {
                    ret = -1;
            } else {
                    ...
                    ret = 0;
            }
    
            return ret;
    }

.. rst-class:: non-compliant-code

   Non-compliant example::

       int32_t foo(char *ptr) {
               if (ptr == NULL) {
                       return -1;
               }
               ...
               return 0;
       }


C-CU-03: All code shall be reachable
====================================

Compliant example::

    uint32_t func_showcase(void)
    {
            uint32_t showcase = 32U;
    
            printf("showcase: %d \n", showcase);
            return showcase;
    }

.. rst-class:: non-compliant-code

   Non-compliant example::

       uint32_t func_showcase(void)
       {
               uint32_t showcase = 32U;
       
               return showcase;
               printf("showcase: %d \n", showcase);
       }


C-CU-04: Cyclomatic complexity shall be less than 20
====================================================

A function with cyclomatic complexity greater than 20 shall be split
into multiple sub-functions to simplify the function logic.

Compliant example::

    bool is_even_number(uint32_t param)
    {
            bool even = false;
    
            if ((param & 0x1U) == 0U) {
                    even = true;
            }
    
            return even;
    }
    
    uint32_t func_showcase(uint32_t param)
    {
            uint32_t ret;
    
            if (param >= 20U) {
                    ret = 20U;
            } else if (is_even_number(param)) {
                    ret = 10U;
            } else {
                    ret = 0U;
            }
    
            return ret;
    }

.. rst-class:: non-compliant-code

   Non-compliant example::

       uint32_t func_showcase(uint32_t param)
       {
               uint32_t ret;
       
               if (param >= 20U) {
                       ret = 20U;
               }
       
               if ((param == 0U) || (param == 2U) || (param == 4U) || (param == 6U) ||
                       (param == 8U) || (param == 10U) || (param == 12U) || (param == 14U) ||
                       (param == 16U) || (param == 18U)) {
                       ret = 10U;
               }
       
               if ((param == 1U) || (param == 3U) || (param == 5U) || (param == 7U) ||
                       (param == 9U) || (param == 11U) || (param == 13U) || (param == 15U) ||
                       (param == 17U) || (param == 19U)) {
                       ret = 0U;
               }
       
               return ret;
       }



Declarations and Initialization
*******************************

C-DI-01: Variable shall be used after its initialization
========================================================

Compliant example::

    uint32_t a, b;
    
    a = 0U;
    b = a;

.. rst-class:: non-compliant-code

   Non-compliant example::

       uint32_t a, b;
       
       b = a;


C-DI-02: Function shall be called after its declaration
=======================================================

Compliant example::

    static void showcase_2(void)
    {
            /* main body */
    }
    
    static void showcase_1(void)
    {
            showcase_2(void);
    }

.. rst-class:: non-compliant-code

   Non-compliant example::

       static void showcase_1(void)
       {
               showcase_2(void);
       }
       
       static void showcase_2(void)
       {
               /* main body */
       }


C-DI-03: The initialization statement shall not be skipped
==========================================================

Compliant example::

            uint32_t showcase;
    
            showcase = 0U;
            goto increment_ten;
            showcase += 20U;
    
    increment_ten:
            showcase += 10U;

.. rst-class:: non-compliant-code

   Non-compliant example::

               uint32_t showcase;
       
               goto increment_ten;
               showcase = 0U;
               showcase += 20U;
       
       increment_ten:
               showcase += 10U;


C-DI-04: The initialization of struct shall be enclosed with brackets
=====================================================================

Compliant example::

    struct struct_showcase_sub
    {
            uint32_t temp_1;
            uint32_t temp_2;
    };
    
    struct struct_showcase
    {
            uint32_t temp_3;
            struct struct_showcase_sub temp_struct;
    };
    
    struct struct_showcase showcase = {32U, {32U, 32U}};

.. rst-class:: non-compliant-code

   Non-compliant example::

       struct struct_showcase_sub
       {
               uint32_t temp_1;
               uint32_t temp_2;
       };
       
       struct struct_showcase
       {
               uint32_t temp_3;
               struct struct_showcase_sub temp_struct;
       };
       
       struct struct_showcase showcase = {32U, 32U, 32U};


C-DI-05: The array size shall be specified explicitly
=====================================================

Compliant example::

    uint32_t showcase[2] = {0U, 1U};

.. rst-class:: non-compliant-code

   Non-compliant example::

       uint32_t showcase[] = {0U, 1U};


C-DI-06: Global variables shall only be declared once
=====================================================

Global variables shall only be declared once with the following exception.
A global variable may be declared twice, if one declaration is in a header file
with extern specifier, and the other one is in a source file without extern
specifier.

Compliant example::

    /* In `showcase.h` */
    extern uint32_t showcase;
    
    /* In `showcase.c`: */
    /* global variable */
    uint32_t showcase = 32U;
    
    void func_showcase(void)
    {
            showcase++;
    }

.. rst-class:: non-compliant-code

   Non-compliant example::

       /* In `showcase.c`: */
       /* global variable */
       uint32_t showcase;
       uint32_t showcase = 32U;
       
       void func_showcase(void)
       {
               showcase++;
       }


C-DI-07: An array shall be fully initialized
============================================

Compliant example::

    uint32_t showcase_array[5] = {0, 1, 2, 3, 4};

.. rst-class:: non-compliant-code

   Non-compliant example::

       uint32_t showcase_array[5] = {0, 1};


C-DI-08: An array declaration shall use a constant for the size
===============================================================

Compliant example::

    uint32_t array_showcase[10];

.. rst-class:: non-compliant-code

   Non-compliant example::

       uint32_t array_size = 10U;
       uint32_t array_showcase[array_size];



Functions
*********

C-FN-01: A non-void function shall have return statement
========================================================

Compliant example::

    uint32_t showcase(uint32_t param)
    {
            printf("param: %d\n", param);
            return param;
    }

.. rst-class:: non-compliant-code

   Non-compliant example::

       uint32_t showcase(uint32_t param)
       {
               printf("param: %d\n", param);
       }


C-FN-02: A non-void function shall have return value rather than empty return
=============================================================================

Compliant example::

    uint32_t showcase(uint32_t param)
    {
            printf("param: %d\n", param);
            return param;
    }

.. rst-class:: non-compliant-code

   Non-compliant example::

       uint32_t showcase(uint32_t param)
       {
               printf("param: %d\n", param);
               return;
       }


C-FN-03: A non-void function shall return value on all paths
============================================================

Compliant example::

    uint32_t showcase(uint32_t param)
    {
            if (param < 10U) {
                    return 10U;
            } else {
                    return param;
            }
    }

.. rst-class:: non-compliant-code

   Non-compliant example::

       uint32_t showcase(uint32_t param)
       {
               if (param < 10U) {
                       return 10U;
               } else {
                       return;
               }
       }


C-FN-04: The return value of a void-returning function shall not be used
========================================================================

Compliant example::

    void showcase_1(uint32_t param)
    {
            printf("param: %d\n", param);
    }
    
    void showcase_2(void)
    {
            uint32_t a;
    
            showcase_1(0U);
            a = 0U;
    }

.. rst-class:: non-compliant-code

   Non-compliant example::

       void showcase_1(uint32_t param)
       {
               printf("param: %d\n", param);
       }
       
       void showcase_2(void)
       {
               uint32_t a;
       
               a = showcase_1(0U);
       }


C-FN-05: Parameter passed by pointer to a function shall not be reassigned
==========================================================================

Compliant example::

    void func_showcase(uint32_t *param_ptr)
    {
            uint32_t *local_ptr = param_ptr;
    
            local_ptr++;
            printf("%d \n", *local_ptr);
    }

.. rst-class:: non-compliant-code

   Non-compliant example::

       void func_showcase(uint32_t *param_ptr)
       {
               param_ptr++;
               printf("%d \n", *param_ptr);
       }


C-FN-06: Parameter passed by value to a function shall not be modified directly
===============================================================================

Compliant example::

    void func_showcase(uint32_t param)
    {
            uint32_t local = param;
    
            local++;
            printf("%d \n", local);
    }

.. rst-class:: non-compliant-code

   Non-compliant example::

       void func_showcase(uint32_t param)
       {
               param++;
               printf("%d \n", param);
       }


C-FN-07: Non-static function shall be declared in header file
=============================================================

Compliant example::

    /* In `showcase.h`: */
    uint32_t func_showcase(uint32_t param);
    
    /* In `showcase.c`: */
    #include "showcase.h"
    
    uint32_t func_showcase(uint32_t param)
    {
            return param;
    }

.. rst-class:: non-compliant-code

   Non-compliant example::

       /* There is no `showcase.h`. */
       
       /* In `showcase.c`: */
       uint32_t func_showcase(uint32_t param)
       {
               return param;
       }


C-FN-08: All static functions shall be used within the file they are declared
=============================================================================

Unlike global functions in C, access to a static function is restricted to the
file where it is declared. Therefore, a static function shall be used in the
file where it is declared, either called explicitly or indirectly via its
address. Otherwise, the static function shall be removed.

Compliant example::

    static void func_showcase(uint32_t param)
    {
            printf("param %d \n", param);
    }
    
    void main(void)
    {
            func_showcase(10U);
    }

.. rst-class:: non-compliant-code

   Non-compliant example::

       /* func_showcase is not called explicitly or accessed via the address */
       static void func_showcase(uint32_t param)
       {
               printf("param %d \n", param);
       }


C-FN-09: The formal parameter name of a function shall be consistent
====================================================================

The formal parameter name of a function shall be the same between its
declaration and definition.

Compliant example::

    /* In `showcase.h`: */
    uint32_t func_showcase(uint32_t param);
    
    /* In `showcase.c`: */
    #include "showcase.h"
    
    uint32_t func_showcase(uint32_t param)
    {
            return param;
    }

.. rst-class:: non-compliant-code

   Non-compliant example::

       /* In `showcase.h`: */
       uint32_t func_showcase(uint32_t param);
       
       /* In `showcase.c`: */
       #include "showcase.h"
       
       uint32_t func_showcase(uint32_t param_1)
       {
               return param_1;
       }


C-FN-10: The formal parameter type of a function shall be consistent
====================================================================

The formal parameter type of a function shall be the same between its
declaration and definition.

Compliant example::

    /* In `showcase.h`: */
    uint32_t func_showcase(uint32_t param);
    
    /* In `showcase.c`: */
    #include "showcase.h"
    
    uint32_t func_showcase(uint32_t param)
    {
            return param;
    }

.. rst-class:: non-compliant-code

   Non-compliant example::

       /* In `showcase.h`: */
       uint32_t func_showcase(uint64_t param);
       
       /* In `showcase.c`: */
       #include "showcase.h"
       
       uint32_t func_showcase(uint32_t param)
       {
               return param;
       }


C-FN-11: The return type of a function shall be consistent
==========================================================

The return type of a function shall be the same between its declaration and
definition.

Compliant example::

    /* In `showcase.h`: */
    uint32_t func_showcase(uint32_t param);
    
    /* In `showcase.c`: */
    #include "showcase.h"
    
    uint32_t func_showcase(uint32_t param)
    {
            return param;
    }

.. rst-class:: non-compliant-code

   Non-compliant example::

       /* In `showcase.h`: */
       uint64_t func_showcase(uint64_t param);
       
       /* In `showcase.c`: */
       #include "showcase.h"
       
       uint32_t func_showcase(uint32_t param)
       {
               return param;
       }


C-FN-12: Banned functions shall not be used
===========================================

The following cases shall be covered:

a) These dynamic memory allocation functions shall not be used: ``calloc``,
   ``malloc``, ``realloc``, and ``free``.  Dynamic memory allocation shall be
   replaced with static memory allocation.
b) The functions ``va_arg``, ``va_start``, and ``va_end`` shall only be used
   within variadic functions (functions taking a variable number of parameters)
   such as ``printf``.

Compliant example::

    uint32_t showcase_array[32];

.. rst-class:: non-compliant-code

   Non-compliant example::

       uint32_t *showcase_ptr = (uint32_t *)malloc(32U * sizeof(uint32_t));


C-FN-13: All declared functions shall have a corresponding definition
=====================================================================

Compliant example::

    /* In `showcase.h`: */
    /* declaration */
    uint32_t func_showcase(uint32_t param);
    
    /* In `showcase.c`: */
    #include "showcase.h"
    
    /* definition */
    uint32_t func_showcase(uint32_t param)
    {
            return param;
    }

.. rst-class:: non-compliant-code

   Non-compliant example::

       /* In `showcase.h`: */
       /* declaration */
       uint32_t func_showcase(uint32_t param);
       
       /* There is no definition of `func_showcase` anywhere in all source files */


C-FN-14: All defined functions shall be used
============================================

All defined functions shall be used, either called explicitly or indirectly via
its address. Otherwise, the function shall be removed. The following case is an
exception. Some extra functions may be kept in order to provide a more complete
library of APIs. These functions may have been implemented but not used
currently. These functions will come in handy in the future. In this case, these
functions may remain.

Compliant example::

    /* In `showcase.h`: */
    uint32_t func_showcase(uint32_t param);
    
    /* In `showcase.c`: */
    #include "showcase.h"
    
    uint32_t func_showcase(uint32_t param)
    {
            return param;
    }
    
    /* In `main.c`: */
    #include "showcase.h"
    
    void main(void)
    {
            uint32_t showcase;
    
            showcase = func_showcase(32U);
    }

.. rst-class:: non-compliant-code

   Non-compliant example::

       /* In `showcase.h`: */
       uint32_t func_showcase(uint32_t param);
       
       /* In `showcase.c`: */
       #include "showcase.h"
       
       /* There is no usage of `func_showcase` anywhere in all source files */
       uint32_t func_showcase(uint32_t param)
       {
               return param;
       }


C-FN-15: A function shall not return a pointer to a local object
================================================================

A function shall not return a pointer to a local object, either directly or
within a returned structure or array.

Compliant example::

    struct struct_showcase
    {
            uint32_t temp_32;
            uint64_t temp_64;
    };
    
    struct struct_showcase func_showcase(void)
    {
            struct struct_showcase showcase;
            uint32_t showcase_u32 = 32U;
            uint64_t showcase_u64 = 64UL;
    
            showcase.temp_32 = showcase_u32;
            showcase.temp_64 = showcase_u64;
    
            return showcase;
    }

.. rst-class:: non-compliant-code

   Non-compliant example::

       struct struct_showcase
       {
               uint32_t *temp_32;
               uint64_t *temp_64;
       };
       
       struct struct_showcase func_showcase(void)
       {
               struct struct_showcase showcase;
               uint32_t showcase_u32 = 32U;
               uint64_t showcase_u64 = 64UL;
       
               showcase.temp_32 = &showcase_u32;
               showcase.temp_64 = &showcase_u64;
       
               return showcase;
       }
       


C-FN-16: Mixed-use of C code and assembly code in a single function shall not be allowed
========================================================================================

A function with mixed-use of C code and assembly code shall be split into
multiple sub-functions to separate the usage of C code and assembly code.

Compliant example::

    void asm_hlt(void)
    {
            asm volatile ("hlt");
    }
    
    void func_showcase(void)
    {
            bool showcase_flag = true;
    
            if (showcase_flag) {
                    asm_hlt();
            }
    }

.. rst-class:: non-compliant-code

   Non-compliant example::

       void func_showcase(void)
       {
               bool showcase_flag = true;
       
               if (showcase_flag) {
                       asm volatile ("hlt");
               }
       }


C-FN-17: The return value of a non-void function shall either be used or discarded
==================================================================================

The return value of a non-void function shall either be used or discarded
explicitly via (void). If the return value contains the error code, this return
value shall be checked in all possible paths.

Compliant example::

    /** Indicates that argument is not valid. */
    #define EINVAL                22
    
    int32_t func_showcase(uint32_t param)
    {
            int32_t error;
    
            if (param < 32U) {
                    error = 0;
            } else {
                    error = -EINVAL;
            }
    
            return error;
    }
    
    void main(uint32_t index)
    {
            int32_t error;
            uint32_t test;
            uint32_t array_showcase[32];
    
            error = func_showcase(index);
    
            if (error == 0) {
                    test = array_showcase[index];
            }
    }

.. rst-class:: non-compliant-code

   Non-compliant example::

       /** Indicates that argument is not valid. */
       #define EINVAL                22
       
       int32_t func_showcase(uint32_t param)
       {
               int32_t error;
       
               if (param < 32U) {
                       error = 0;
               } else {
                       error = -EINVAL;
               }
       
               return error;
       }
       
       void main(uint32_t index)
       {
               int32_t error;
               uint32_t test;
               uint32_t array_showcase[32];
       
               error = func_showcase(index);
       
               test = array_showcase[index];
       }


C-FN-18: The array size shall be valid if the array is function input parameter
===============================================================================

This is to guarantee that the destination array has sufficient space for the
operation, such as copy, move, compare and concatenate.

Compliant example::

    void showcase(uint32_t array_source[16])
    {
            uint32_t array_destination[16];
    
            (void)memcpy(array_destination, array_source, 16U);
    }

.. rst-class:: non-compliant-code

   Non-compliant example::

       void showcase(uint32_t array_source[32])
       {
               uint32_t array_destination[16];
       
               (void)memcpy(array_destination, array_source, 32U);
       }


C-FN-19: Recursion shall not be used in function calls
======================================================

Compliant example::

    uint32_t func_showcase(uint32_t param) {
            uint32_t mult = 1;
            uint32_t i;
    
            for (i = param; i > 0U; i--) {
                    mult = mult * i;
            }
            
            return mult;
    }

.. rst-class:: non-compliant-code

   Non-compliant example::

       uint32_t func_showcase(uint32_t param) {
               uint32_t mult = 1;
       
               if (param > 0U) {
                       mult = param * func_showcase(param - 1);
               }
               
               return mult;
       }



Statements
**********

C-ST-01: The condition of a selection or iteration statement shall not be constant
==================================================================================

The condition of a selection or iteration statement shall not be constant with
the following exception, `do { ... } while (0)` shall be allowed if it is used
in a MACRO.

Compliant example::

    void func_showcase(uint32_t param)
    {
            if (param != 0U) {
                    printf("param %d \n", param);
            }
    }

.. rst-class:: non-compliant-code

   Non-compliant example::

       void func_showcase(uint32_t param)
       {
               if (false) {
                       printf("param %d \n", param);
               }
       }


C-ST-02: The loop body shall be enclosed with brackets
======================================================

Compliant example::

    uint32_t i;
    
    for (i = 0U; i < 5U; i++) {
            printf("count: %d \n", i);
    }

.. rst-class:: non-compliant-code

   Non-compliant example::

       uint32_t i;
       
       for (i = 0U; i < 5U; i++)
               printf("count: %d \n", i);


C-ST-03: Infinite loop shall not exist
======================================

Every path in the iteration loop shall have the chance to exit.

Compliant example::

    uint32_t count = 10U;
    bool showcase_flag = false;
    
    while (count > 5U)
    {
            if (showcase_flag) {
                    count--;
            } else {
                    count = count - 2U;
            }
    }

.. rst-class:: non-compliant-code

   Non-compliant example::

       uint32_t count = 10U;
       bool showcase_flag = false;
       
       while (count > 5U)
       {
               if (showcase_flag) {
                       count--;
               }
       }


C-ST-04: The else statement shall not be empty if it is following an else if
============================================================================

Either a non-null statement or a comment shall be included in the else
statement. This is to guarantee that the developers have considered all of the
possible cases.

Compliant example::

    uint32_t param, showcase;
    
    if (param < 10U) {
            showcase = 10U;
    } else if (param < 20U) {
            showcase = 20U;
    } else {
            showcase = 30U;
    }

.. rst-class:: non-compliant-code

   Non-compliant example::

       uint32_t param, showcase;
       
       if (param < 10U) {
               showcase = 10U;
       } else if (param < 20U) {
               showcase = 20U;
       } else {
       }


C-ST-05: A switch statement shall have the default statement
============================================================

This is to guarantee that the developers have considered all of the possible
cases.

Compliant example::

    char showcase;
    
    switch (showcase) {
    case 'a':
            /* do something */
            break;
    case 'A':
            /* do something */
            break;
    default:
            /* do something */
            break;
    }

.. rst-class:: non-compliant-code

   Non-compliant example::

       char showcase;
       
       switch (showcase) {
       case 'a':
               /* do something */
               break;
       case 'A':
               /* do something */
               break;
       }


C-ST-06: Every switch clause shall be terminated with a break statement
=======================================================================

Falling through a case shall not be allowed.

Compliant example::

    char showcase;
    
    switch (showcase) {
    case 'a':
            /* do something */
            break;
    case 'A':
            /* do something */
            break;
    default:
            /* do something */
            break;
    }

.. rst-class:: non-compliant-code

   Non-compliant example::

       char showcase;
       
       switch (showcase) {
       case 'a':
               /* do something */
       case 'A':
               /* do something */
       default:
               /* do something */
               break;
       }


C-ST-07: The for loop counter shall not be changed inside the loop body
=======================================================================

Compliant example::

    uint32_t i;
    
    for (i = 0U; i < 5U; i++) {
            printf("count: %d \n", i);
    }

.. rst-class:: non-compliant-code

   Non-compliant example::

       uint32_t i;
       
       for (i = 0U; i < 5U; i++) {
               printf("count: %d \n", i);
               i++;
       }


C-ST-08:  'goto' statement shall not be used
============================================

Compliant example::

    uint32_t showcase(uint32_t param)
    {
            uint32_t ret;
    
            if (param < 10U) {
                    ret = 10U;
            } else {
                    ret = param;
                    /* do something */
            }
    
            return ret;
    }

.. rst-class:: non-compliant-code

   Non-compliant example::

       uint32_t showcase(uint32_t param)
       {
               uint32_t ret;
       
               if (param < 10U) {
                       ret = 10U;
                       goto done;
               } else {
                       ret = param;
               }
       
               /* do something */
       
       done:
               return ret;
       }



Expressions
***********

C-EP-01: The initialization expression of a for loop shall be simple
====================================================================

The initialization expression of a for loop shall only be used to initialize the
loop counter. All other operations shall not be allowed.

Compliant example::

    uint32_t i;
    
    for (i = 0U; i < 5U; i++) {
            printf("count: %d \n", i);
    }

.. rst-class:: non-compliant-code

   Non-compliant example::

       uint32_t i;
       uint32_t showcase = 0U;
       
       for (i = 0U, showcase = 10U; i < 5U; i++) {
               printf("count: %d \n", i);
       }


C-EP-02: The controlling expression of a for loop shall not be empty
====================================================================

Compliant example::

    uint32_t i;
    
    for (i = 0U; i < 5U; i++) {
            printf("count: %d \n", i);
    }

.. rst-class:: non-compliant-code

   Non-compliant example::

       uint32_t i;
       
       for (i = 0U; ; i++) {
               printf("count: %d \n", i);
               if (i > 4U) {
                       break;
               }
       }


C-EP-03: The third expression of a for loop shall be simple
===========================================================

The third expression of a for loop shall only be used to increase or decrease
the loop counter with the following operators, ++, --, +=, or -=. All other
operations shall not be allowed.

Compliant example::

    uint32_t i;
    
    for (i = 0U; i < 5U; i++) {
            printf("count: %d \n", i);
    }

.. rst-class:: non-compliant-code

   Non-compliant example::

       uint32_t i;
       uint32_t showcase = 0U;
       
       for (i = 0U; i < 5U; i++, showcase++) {
               printf("count: %d \n", i);
       }


C-EP-04: The evaluation order of an expression shall not influence the result
=============================================================================

Compliant example::

    uint32_t showcase = 0U;
    uint32_t showcase_test = 10U;
    
    showcase++;
    showcase_test = showcase_test + showcase;

.. rst-class:: non-compliant-code

   Non-compliant example::

       uint32_t showcase = 0U;
       uint32_t showcase_test = 10U;
       
       showcase_test = showcase_test + ++showcase;


C-EP-05: Parentheses shall be used to set the operator precedence explicitly
============================================================================

Compliant example::

    uint32_t showcase_u32_1 = 0U;
    uint32_t showcase_u32_2 = 0xFFU;
    uint32_t showcase_u32_3;
    
    showcase_u32_3 = showcase_u32_1 * (showcase_u32_2 >> 4U);

.. rst-class:: non-compliant-code

   Non-compliant example::

       uint32_t showcase_u32_1 = 0U;
       uint32_t showcase_u32_2 = 0xFU;
       uint32_t showcase_u32_3;
       
       showcase_u32_3 = showcase_u32_1 * showcase_u32_2 >> 4U;


C-EP-06: Overflow shall not be allowed
======================================

Compliant example::

    uint8_t showcase = 255U;

.. rst-class:: non-compliant-code

   Non-compliant example::

       uint8_t showcase = 255U + 1U;


C-EP-07: Negation shall not be performed on unsigned expression
===============================================================

Compliant example::

    int32_t showcase = -10;

.. rst-class:: non-compliant-code

   Non-compliant example::

       int32_t showcase = -10U;


C-EP-08: The address of an object shall not be assigned to another object whose lifetime is longer
==================================================================================================

Compliant example::

    void func_showcase(void)
    {
            uint32_t showcase_local = 32U;
            uint32_t *showcase_ptr_local;
    
            showcase_ptr_local = &showcase_local;
            printf("*showcase_ptr_local %d \n", *showcase_ptr_local);
    }

.. rst-class:: non-compliant-code

   Non-compliant example::

       uint32_t *showcase_ptr_global;
       
       void func_showcase(void)
       {
               uint32_t showcase_local = 32U;
               uint32_t *showcase_ptr_local;
       
               showcase_ptr_local = &showcase_local;
               showcase_ptr_global = showcase_ptr_local;
       }
       
       void main(void)
       {
               func_showcase();
               printf("*showcase_ptr_global %d \n", *showcase_ptr_global);
       }


C-EP-09: sizeof operator shall not be performed on an array function parameter
==============================================================================

When an array is used as the function parameter, the array address is passed.
Thus, the return value of the sizeof operation is the pointer size rather than
the array size.

Compliant example::

    #define SHOWCASE_SIZE 32U
    
    void showcase(uint32_t array_source[SHOWCASE_SIZE])
    {
            uint32_t num_bytes = SHOWCASE_SIZE * sizeof(uint32_t);
    
            printf("num_bytes %d \n", num_bytes);
    }

.. rst-class:: non-compliant-code

   Non-compliant example::

       #define SHOWCASE_SIZE 32U
       
       void showcase(uint32_t array_source[SHOWCASE_SIZE])
       {
               uint32_t num_bytes = sizeof(array_source);
       
               printf("num_bytes %d \n", num_bytes);
       }


C-EP-10: Argument of strlen shall end with a null character
===========================================================

Compliant example::

    uint32_t size;
    char showcase[3] = {'0', '1', '\0'};
    
    size = strlen(showcase);

.. rst-class:: non-compliant-code

   Non-compliant example::

       uint32_t size;
       char showcase[2] = {'0', '1'};
       
       size = strlen(showcase);


C-EP-11: Two strings shall not be copied to each other if they have memory overlap
==================================================================================

Compliant example::

    char *str_source = "showcase";
    char str_destination[32];
    
    (void)strncpy(str_destination, str_source, 8U);

.. rst-class:: non-compliant-code

   Non-compliant example::

       char *str_source = "showcase";
       char *str_destination = &str_source[1];
       
       (void)strncpy(str_destination, str_source, 8U);


C-EP-12: memcpy shall not be performed on objects with overlapping memory
=========================================================================

Compliant example::

    char *str_source = "showcase";
    char str_destination[32];
    
    (void)memcpy(str_destination, str_source, 8U);

.. rst-class:: non-compliant-code

   Non-compliant example::

       char str_source[32];
       char *str_destination = &str_source[1];
       
       (void)memcpy(str_destination, str_source, 8U);


C-EP-13: Assignment shall not be performed between variables with overlapping storage
=====================================================================================

Compliant example::

    union union_showcase
    {
            uint8_t data_8[4];
            uint16_t data_16[2];
    };
    
    union union_showcase showcase;
    
    showcase.data_16[0] = 0U;
    showcase.data_8[3] = (uint8_t)showcase.data_16[0];

.. rst-class:: non-compliant-code

   Non-compliant example::

       union union_showcase
       {
               uint8_t data_8[4];
               uint16_t data_16[2];
       };
       
       union union_showcase showcase;
       
       showcase.data_16[0] = 0U;
       showcase.data_8[0] = (uint8_t)showcase.data_16[0];


C-EP-14: The destination object shall have sufficient space for operation
=========================================================================

The destination object shall have sufficient space for operation, such as copy,
move, compare and concatenate. Otherwise, data corruption may occur.

Compliant example::

    uint32_t array_source[32];
    uint32_t array_destination[32];
    
    (void)memcpy(array_destination, array_source, 32U);

.. rst-class:: non-compliant-code

   Non-compliant example::

       uint32_t array_source[32];
       uint32_t array_destination[16];
       
       (void)memcpy(array_destination, array_source, 32U);


C-EP-15: The size param to memcpy/memset shall be valid
=======================================================

The size param shall not be larger than either the source size or destination
size. Otherwise, data corruption may occur.

Compliant example::

    #define SHOWCASE_BYTES (32U * sizeof(uint32_t))
    
    uint32_t array_source[32];
    
    (void)memset(array_source, 0U, SHOWCASE_BYTES);

.. rst-class:: non-compliant-code

   Non-compliant example::

       #define SHOWCASE_BYTES (32U * sizeof(uint32_t))
       
       uint32_t array_source[32];
       
       (void)memset(array_source, 0U, 2U * SHOWCASE_BYTES);


C-EP-16: The denominator of a divide shall not be zero
======================================================

The denominator of a divide shall be checked before use.

Compliant example::

    uint32_t numerator = 32U;
    uint32_t denominator = 0U;
    
    if (denominator != 0U) {
            uint32_t quotient = numerator / denominator;
    }

.. rst-class:: non-compliant-code

   Non-compliant example::

       uint32_t numerator = 32U;
       uint32_t denominator = 0U;
       
       uint32_t quotient = numerator / denominator;


C-EP-17: A NULL pointer shall not be dereferenced
=================================================

A pointer shall be checked before use.

Compliant example::

    uint32_t *showcase_ptr = NULL;
    
    if (showcase_ptr != NULL) {
            uint32_t showcase = *showcase_ptr;
    }

.. rst-class:: non-compliant-code

   Non-compliant example::

       uint32_t *showcase_ptr = NULL;
       
       uint32_t showcase = *showcase_ptr;


C-EP-18: A string literal shall not be modified
===============================================

Compliant example::

    const char *showcase = "showcase";
    
    printf("%s \n", showcase);

.. rst-class:: non-compliant-code

   Non-compliant example::

       char *showcase = "showcase";
       
       showcase[0] = 'S';
       printf("%s \n", showcase);


C-EP-19:  ++ or -- operation shall be used with restrictions
============================================================

Only the following cases shall be allowed:

a) ++ or -- operation shall be allowed if it is used alone in the expression;
b) ++ or -- operation shall be allowed if it is used as the third expression of
   a for loop.

Compliant example::

    uint32_t showcase = 0U;
    
    showcase++;

.. rst-class:: non-compliant-code

   Non-compliant example::

       uint32_t showcase = 0U;
       uint32_t showcase_test;
       
       showcase_test = showcase++;


C-EP-20: Array indexing shall be in-bounds
==========================================

An array index value shall be between zero (for the first element) and the array
size minus one (for the last element). Out-of-bound array references are an
undefined behavior and shall be avoided.

Compliant example::

    char showcase_array[4] = {'s', 'h', 'o', 'w'};
    
    char showcase = showcase_array[0];

.. rst-class:: non-compliant-code

   Non-compliant example::

       char showcase_array[4] = {'s', 'h', 'o', 'w'};
       
       char showcase = showcase_array[10];


C-EP-21: The comma operator shall not be used
=============================================

Compliant example::

    uint32_t showcase_a = 10U;
    uint32_t showcase_b = 20U;
    
    showcase_a++;
    showcase_b++;

.. rst-class:: non-compliant-code

   Non-compliant example::

       uint32_t showcase_a = 10U;
       uint32_t showcase_b = 20U;
       
       showcase_a++, showcase_b++;



Types
*****

C-TY-01: The function return value shall be consistent with the declared return type
====================================================================================

Compliant example::

    uint32_t func_showcase(uint32_t param)
    {
            if (param < 10U) {
                    return 10U;
            } else {
                    return 20U;
            }
    }

.. rst-class:: non-compliant-code

   Non-compliant example::

       uint32_t func_showcase(uint32_t param)
       {
               if (param < 10U) {
                       return 10U;
               } else {
                       return -1;
               }
       }


C-TY-02: The operands of bit operations shall be unsigned
=========================================================

Compliant example::

    uint32_t showcase = 32U;
    uint32_t mask = 0xFU;
    
    showcase = showcase & mask;

.. rst-class:: non-compliant-code

   Non-compliant example::

       uint32_t showcase = 32U;
       int32_t mask = -1;
       
       showcase = showcase & mask;


C-TY-03: Mixed-use between Boolean values and integers shall not be allowed
===========================================================================

Some detailed rules are listed below:

a) The operands of the arithmetic operation shall be integers;
b) The operands of the logical operation shall be Boolean values;
c) The controlling expression of a selection or iteration statement shall be
   Boolean;
d) A Boolean type expression shall be used where Boolean is expected.

Compliant example::

    bool showcase_flag = true;
    uint32_t exp = 32U;
    uint32_t cond_exp = 64U;
    
    uint32_t showcase = showcase_flag ? exp : cond_exp;

.. rst-class:: non-compliant-code

   Non-compliant example::

       uint32_t showcase_flag = 1U;
       uint32_t exp = 32U;
       uint32_t cond_exp = 64U;
       
       uint32_t showcase = showcase_flag ? exp : cond_exp;


C-TY-04: The enum shall not be used for arithmetic operations
=============================================================

Only the following operations on enum shall be allowed:

a) enum assignment shall be allowed if the operands of = operation have the same
   enum type;
b) enum comparison shall be allowed, including the operators ==, !=, >, <, >=,
   and <=.

Compliant example::

    enum enum_showcase {
            ENUM_SHOWCASE_0,
            ENUM_SHOWCASE_1
    };
    
    enum enum_showcase showcase_0 = ENUM_SHOWCASE_0;
    enum enum_showcase showcase_1 = showcase_0;

.. rst-class:: non-compliant-code

   Non-compliant example::

       enum enum_showcase {
               ENUM_SHOWCASE_0,
               ENUM_SHOWCASE_1
       };
       
       enum enum_showcase showcase_0 = ENUM_SHOWCASE_0;
       enum enum_showcase showcase_1 = showcase_0 + 1U;


C-TY-05: static keyword shall not be used in an array index declaration
=======================================================================

Compliant example::

    char showcase[2] = {'0', '1'};
    char chr;
    
    chr = showcase[1];

.. rst-class:: non-compliant-code

   Non-compliant example::

       char showcase[2] = {'0', '1'};
       char chr;
       
       chr = showcase[static 1];


C-TY-06: A pointer shall point to const object if the object is not modified
============================================================================

Compliant example::

    void func_showcase(const uint32_t *ptr)
    {
            printf("value: %d \n", *ptr);
    }

.. rst-class:: non-compliant-code

   Non-compliant example::

       void func_showcase(uint32_t *ptr)
       {
               printf("value: %d \n", *ptr);
       }


C-TY-07: The expressions type of ternary operation shall be consistent
======================================================================

Compliant example::

    bool showcase_flag = true;
    uint32_t exp = 32U;
    uint32_t cond_exp = 64U;
    
    uint32_t showcase = showcase_flag ? exp : cond_exp;

.. rst-class:: non-compliant-code

   Non-compliant example::

       bool showcase_flag = true;
       int32_t exp = -1;
       uint32_t cond_exp = 64U;
       
       uint32_t showcase = showcase_flag ? exp : cond_exp;


C-TY-08: The struct field type shall be consistent
==================================================

The struct field type shall be consistent between its definition and
initialization.

Compliant example::

    struct struct_showcase
    {
            uint32_t temp_32;
            uint64_t temp_64;
    };
    
    struct struct_showcase showcase = {32U, 64UL};

.. rst-class:: non-compliant-code

   Non-compliant example::

       struct struct_showcase
       {
               uint32_t temp_32;
               uint64_t temp_64;
       };
       
       struct struct_showcase showcase = {32U, -1};


C-TY-09: The type used in switch statement shall be consistent
==============================================================

The type shall be consistent between the case expression and the controlling
expression of switch statement.

Compliant example::

    enum enum_showcase {
            ENUM_SHOWCASE_0,
            ENUM_SHOWCASE_1,
            ENUM_SHOWCASE_2
    };
    
    enum enum_showcase showcase;
    
    switch (showcase) {
    case ENUM_SHOWCASE_0:
            /* showcase */
            break;
    case ENUM_SHOWCASE_1:
            /* showcase */
            break;
    default:
            /* showcase */
            break;
    }

.. rst-class:: non-compliant-code

   Non-compliant example::

       enum enum_showcase {
               ENUM_SHOWCASE_0,
               ENUM_SHOWCASE_1,
               ENUM_SHOWCASE_2
       };
       
       enum enum_showcase showcase;
       
       switch (showcase) {
       case ENUM_SHOWCASE_0:
               /* showcase */
               break;
       case 1U:
               /* showcase */
               break;
       default:
               /* showcase */
               break;
       }


C-TY-10: const qualifier shall not be discarded in cast operation
=================================================================

Compliant example::

    const uint32_t *showcase_const;
    const uint32_t *showcase = showcase_const;

.. rst-class:: non-compliant-code

   Non-compliant example::

       const uint32_t *showcase_const;
       uint32_t *showcase = (uint32_t *)showcase_const;


C-TY-11: A variable shall be declared as static if it is only used in the file where it is declared
===================================================================================================

Compliant example::

    /* In `showcase.c`: */
    /* `showcase` is only in `showcase.c` */
    static uint32_t showcase;

.. rst-class:: non-compliant-code

   Non-compliant example::

       /* In `showcase.c`: */
       /* `showcase` is only in `showcase.c` */
       uint32_t showcase;


C-TY-12: All type conversions shall be explicit
===============================================

Implicit type conversions shall not be allowed.

Compliant example::

    uint32_t showcase_u32;
    uint64_t showcase_u64 = 64UL;
    
    showcase_u32 = (uint32_t)showcase_u64;

.. rst-class:: non-compliant-code

   Non-compliant example::

       uint32_t showcase_u32;
       uint64_t showcase_u64 = 64UL;
       
       showcase_u32 = showcase_u64;


C-TY-13: Cast shall be performed on operands rather than arithmetic expressions
===============================================================================

Compliant example::

    uint32_t showcase_u32_1 = 10U;
    uint32_t showcase_u32_2 = 10U;
    uint64_t showcase_u64;
    
    showcase_u64 = (uint64_t)showcase_u32_1 + (uint64_t)showcase_u32_2;

.. rst-class:: non-compliant-code

   Non-compliant example::

       uint32_t showcase_u32_1 = 10U;
       uint32_t showcase_u32_2 = 10U;
       uint64_t showcase_u64;
       
       showcase_u64 = (uint64_t)(showcase_u32_1 + showcase_u32_2);


C-TY-14: A complex integer expression shall not be cast to types other than integer
===================================================================================

Compliant example::

    /* 0x61 is 'a' in ASCII Table */
    uint32_t showcase_u32;
    char showcase_char;
    
    showcase_u32 = 0x61U + 1U;
    showcase_char = (char)showcase_u32;

.. rst-class:: non-compliant-code

   Non-compliant example::

       /* 0x61 is 'a' in ASCII Table */
       uint32_t showcase_u32;
       char showcase_char;
       
       showcase_u32 = 0x61U;
       showcase_char = (char)(showcase_u32 + 1U);


C-TY-15: Integer shall not be used when a character is expected
===============================================================

Compliant example::

    char showcase;
    
    switch (showcase) {
    case 'a':
            /* do something */
            break;
    case 'A':
            /* do something */
            break;
    default:
            break;
    }

.. rst-class:: non-compliant-code

   Non-compliant example::

       char showcase;
       
       switch (showcase) {
       /* 0x61 is 'a' in ASCII Table */
       case 0x61:
               /* do something */
               break;
       case 'A':
               /* do something */
               break;
       default:
               break;
       }


C-TY-16: A pointer shall not be cast to any other types
=======================================================

Compliant example::

    uint64_t *showcase_ptr;
    
    uint64_t showcase = *showcase_ptr;

.. rst-class:: non-compliant-code

   Non-compliant example::

       uint64_t *showcase_ptr;
       
       uint64_t showcase = (uint64_t)showcase_ptr;


C-TY-17: A pointer shall not be cast from any other types
=========================================================

Only the following pointer assignment shall be allowed:

a) Assignment shall be allowed via the address operator &;
b) Assignment shall be allowed if the objects pointed to by the two pointers are
   of the same type.

Compliant example::

    uint64_t showcase = 10UL;
    
    uint64_t *showcase_ptr = &showcase;

.. rst-class:: non-compliant-code

   Non-compliant example::

       uint64_t showcase = 10UL;
       
       uint64_t *showcase_ptr = (uint64_t *)showcase;


C-TY-18: All types declared by typedef shall be used
====================================================

Typedefs that are not used shall be deleted.

Compliant example::

    typedef unsigned int uint32_t;
    
    uint32_t showcase;

.. rst-class:: non-compliant-code

   Non-compliant example::

       typedef unsigned int uint32_t;
       /* uint32_t_backup is not being used anywhere */
       typedef unsigned int uint32_t_backup;
       
       uint32_t showcase;


C-TY-19: Array indexing shall only be performed on array type
=============================================================

Compliant example::

    char showcase[4] = {'s', 'h', 'o', 'w'};
    
    char chr = showcase[1];

.. rst-class:: non-compliant-code

   Non-compliant example::

       char *showcase = "show";
       
       char chr = showcase[1];


C-TY-20: The actual parameter type shall be the same as the formal parameter type
=================================================================================

Compliant example::

    void func_showcase(uint32_t formal_param)
    {
            printf("formal_param: %d \n", formal_param);
    }
    
    void main(void)
    {
            uint32_t actual_param = 32U;
    
            func_showcase(actual_param);
    }

.. rst-class:: non-compliant-code

   Non-compliant example::

       void func_showcase(uint32_t formal_param)
       {
               printf("formal_param: %d \n", formal_param);
       }
       
       void main(void)
       {
               uint64_t actual_param = 32UL;
       
               func_showcase(actual_param);
       }


C-TY-21: A bit-field shall be a signed integer, unsigned integer, or bool
=========================================================================

All the other types shall not be allowed.

Compliant example::

    struct struct_showcase
    {
            uint8_t function : 3;
            uint8_t device : 5;
            uint8_t bus;
    };

.. rst-class:: non-compliant-code

   Non-compliant example::

       struct struct_showcase
       {
               int function : 3;
               int device : 5;
               int bus;
       };


C-TY-22: Cast shall not be performed on pointers with different object type
===========================================================================

Compliant example::

    struct struct_showcase
    {
            uint32_t *temp_32;
            uint64_t *temp_64;
    };
    
    uint32_t *showcase_ptr_u32;
    struct struct_showcase *showcase_ptr_struct;
    
    showcase_ptr_u32 = showcase_ptr_struct->temp_32;

.. rst-class:: non-compliant-code

   Non-compliant example::

       struct struct_showcase
       {
               uint32_t *temp_32;
               uint64_t *temp_64;
       };
       
       uint32_t *showcase_ptr_u32;
       struct struct_showcase *showcase_ptr_struct;
       
       showcase_ptr_u32 = (uint32_t *)showcase_ptr_struct;


C-TY-23: Assignment on function pointers shall be performed with same type
==========================================================================

Compliant example::

    typedef void (*func_ptr_t)(void);
    
    func_ptr_t func_ptr_a;
    func_ptr_t func_ptr_b;
    
    func_ptr_a = func_ptr_b;

.. rst-class:: non-compliant-code

   Non-compliant example::

       typedef void (*func_ptr_a_t)(void);
       typedef uint32_t (*func_ptr_b_t)(uint32_t param);
       
       func_ptr_a_t func_ptr_a;
       func_ptr_b_t func_ptr_b;
       
       func_ptr_a = func_ptr_b;


C-TY-24: Cast shall not be performed on function pointer
========================================================

Compliant example::

    typedef uint32_t (*func_ptr_t)(uint32_t param);
    
    uint32_t func_showcase(uint32_t param)
    {
            return param;
    }
    
    func_ptr_t func_ptr_showcase;
    func_ptr_showcase = func_showcase;

.. rst-class:: non-compliant-code

   Non-compliant example::

       typedef uint32_t (*func_ptr_t)(uint32_t param);
       
       void func_showcase(uint32_t param)
       {
               printf("param: %d \n", param);
       }
       
       func_ptr_t func_ptr_showcase;
       func_ptr_showcase = (func_ptr_t)func_showcase;


C-TY-25: A string literal shall only be used as const object
============================================================

The following operations shall be covered:

a) If a string literal is assigned to a variable, this variable shall be
   declared with const qualifier;
b) If a string literal is passed as a function parameter, this function
   parameter shall be declared with const qualifier;
c) If a string literal is used as the return value of a function, this function
   return type shall be declared with const qualifier.

Compliant example::

    const char *showcase = "showcase";

.. rst-class:: non-compliant-code

   Non-compliant example::

       char *showcase = "showcase";


C-TY-26: The basic numerical types shall not be used other than in typedefs
===========================================================================

Typedef-name shall be used to replace the usage of basic numerical types. This
is to guarantee the code portability between different compilers and platforms.

Compliant example::

    typedef unsigned int uint32_t;
    
    uint32_t showcase = 32U;

.. rst-class:: non-compliant-code

   Non-compliant example::

       unsigned int showcase = 32U;


C-TY-27: The operands of an assignment operator shall be the same type
======================================================================

Compliant example::

    uint32_t showcase = 32U;

.. rst-class:: non-compliant-code

   Non-compliant example::

       uint32_t showcase = 32UL;


C-TY-28: The operands of arithmetic operations shall be the same type
=====================================================================

Compliant example::

    uint16_t showcase_u16 = 16U;
    uint32_t showcase_u32 = 32U;
    uint64_t showcase_u64 = 64UL;
    
    uint32_t test = (uint32_t)showcase_u16 + showcase_u32 + (uint32_t)showcase_u64;

.. rst-class:: non-compliant-code

   Non-compliant example::

       uint16_t showcase_u16 = 16U;
       uint32_t showcase_u32 = 32U;
       uint64_t showcase_u64 = 64UL;
       
       uint32_t test = showcase_u16 + showcase_u32 + showcase_u64;



Identifiers
***********

C-ID-01: A parameter name shall not be the same as the name of struct, union, enum, variable, or function
=========================================================================================================

Compliant example::

    struct struct_showcase
    {
            char *str_source;
            char *str_destination;
    };
    
    void func_showcase(uint32_t showcase)
    {
            /* main body */
    }

.. rst-class:: non-compliant-code

   Non-compliant example::

       struct showcase
       {
               char *str_source;
               char *str_destination;
       };
       
       void func_showcase(uint32_t showcase)
       {
               /* main body */
       }


C-ID-02: A member name shall not be the same as the name of struct, union, or enum
==================================================================================

Compliant example::

    struct struct_showcase_1
    {
            char *str_source;
            char *str_destination;
    };
    
    struct struct_showcase_2
    {
            uint32_t showcase_1;
            uint32_t showcase_2;
    };

.. rst-class:: non-compliant-code

   Non-compliant example::

       struct showcase_1
       {
               char *str_source;
               char *str_destination;
       };
       
       struct showcase_2
       {
               uint32_t showcase_1;
               uint32_t showcase_2;
       };


C-ID-03: A global variable name shall be unique
===============================================

A global variable name shall not be the same as the name of struct, union, enum,
typedef, function, function parameter, macro, member, enum constant, local
variable, or other global variables.

Compliant example::

    struct struct_showcase
    {
            char *str_source;
            char *str_destination;
    };
    
    /* global variable */
    uint32_t showcase;
    
    void func_showcase(void)
    {
            showcase++;
    }

.. rst-class:: non-compliant-code

   Non-compliant example::

       struct showcase
       {
               char *str_source;
               char *str_destination;
       };
       
       /* global variable */
       uint32_t showcase;
       
       void func_showcase(void)
       {
               showcase++;
       }


C-ID-04: A local variable name shall not be the same as a global variable name
==============================================================================

Compliant example::

    /* global variable */
    uint32_t showcase;
    
    void func_showcase(void)
    {
            uint32_t showcase_local;
    
            showcase_local = 32U;
    }

.. rst-class:: non-compliant-code

   Non-compliant example::

       /* global variable */
       uint32_t showcase;
       
       void func_showcase(void)
       {
               uint32_t showcase;
       
               showcase = 32U;
       }


C-ID-05: The function name shall be unique
==========================================

The function name shall not be the same as the name of struct, union, enum,
typedef, macro, member, enum constant, variable, function parameter, or other
functions.

Compliant example::

    /* global variable */
    uint32_t showcase;
    
    void func_showcase(void)
    {
            /* main body */
    }

.. rst-class:: non-compliant-code

   Non-compliant example::

       /* global variable */
       uint32_t showcase;
       
       void showcase(void)
       {
               /* main body */
       }


C-ID-06: The typedef name shall be unique
=========================================

The typedef name shall be unique and not be used for any other purpose.

Compliant example::

    typedef unsigned int uint32_t;
    
    uint32_t showcase;

.. rst-class:: non-compliant-code

   Non-compliant example::

       typedef unsigned int uint32_t;
       
       uint32_t uint32_t;


C-ID-07: Name defined by developers shall not start with underscore
===================================================================

All names starting with one or two underscores are reserved for use by the
compiler and standard libraries to eliminate potential conflicts with
user-defined names.

Compliant example::

    uint32_t showcase;

.. rst-class:: non-compliant-code

   Non-compliant example::

       uint32_t __showcase;


C-ID-08: A variable name shall not be the same as struct, union or enum
=======================================================================

Compliant example::

    struct struct_showcase
    {
            char *str_source;
            char *str_destination;
    };
    
    uint32_t showcase;

.. rst-class:: non-compliant-code

   Non-compliant example::

       struct showcase
       {
               char *str_source;
               char *str_destination;
       };
       
       uint32_t showcase;


C-ID-09: The typedef name of a numerical type shall indicate the number of bits
===============================================================================

Compliant example::

    typedef unsigned short uint16_t;

.. rst-class:: non-compliant-code

   Non-compliant example::

       typedef unsigned short ushort_t;


C-ID-10: A C keyword shall not be re-defined by a MACRO
=======================================================

Compliant example::

    typedef _Bool bool;

.. rst-class:: non-compliant-code

   Non-compliant example::

       #define _Bool bool


Coding Style
************


C-CS-01: Each line shall contain at most 120 characters
=======================================================

No more than 120 characters shall be on a line, with tab stops every 8
characters. Statements longer than this limit shall be broken into multiple
lines with proper alignment.

Compliant example::

    cpuid(CPUID_EXTEND_FEATURE, &unused,
            &boot_cpu_data.cpuid_leaves[FEAT_7_0_EBX],
            &boot_cpu_data.cpuid_leaves[FEAT_7_0_ECX],
            &boot_cpu_data.cpuid_leaves[FEAT_7_0_EDX]);

.. rst-class:: non-compliant-code

   Non-compliant example::

       cpuid(CPUID_EXTEND_FEATURE, &unused, &boot_cpu_data.cpuid_leaves[FEAT_7_0_EBX], &boot_cpu_data.cpuid_leaves[FEAT_7_0_ECX], &boot_cpu_data.cpuid_leaves[FEAT_7_0_EDX]);


C-CS-02: Each line shall only contain one statement
===================================================

Compliant example::

    if (condition) {
            do_a();
    } else {
            do_b();
    }

.. rst-class:: non-compliant-code

   Non-compliant example::

       if (condition) { do_a();} else {do_b();}


C-CS-03: Tabs shall be used for code indentation
================================================

Spaces are allowed only for indenting comments or aligning statements that span
multiple lines.

Compliant example::

    if (condition) {
            do_a();
    } else {
            do_b();
    }

.. rst-class:: non-compliant-code

   Non-compliant example::

       if (condition) {
         do_a();
       } else {
         do_b();
       }


C-CS-04: Tabs shall be 8 characters wide
========================================

A tab character shall be considered 8-character wide when limiting the line
width.


C-CS-05: Trailing whitespace shall not be allowed at the end of lines
=====================================================================

This rule applies to both spaces and tabs at the end of a line.

Compliant example::

    uint32_t a;
    uint32_t b;
    uint32_t c;

.. rst-class:: non-compliant-code

   Non-compliant example::

       /*
        * The example here uses the char ␣ to stand for the space at the end of the line
        * in order to highlight the non-compliant part.
        */
       uint32_t a;␣␣␣␣
       uint32_t b;␣␣␣␣
       uint32_t c;␣␣␣␣


C-CS-06: A single space shall exist between non-function-like keywords and opening brackets
===========================================================================================

A single space shall exist between a non-function-like keyword and the opening
bracket (either a brace or a parenthesis) that follows. This rule applies to the
keywords 'if', 'else', 'for', 'do', 'while', 'switch' and 'return'.

Compliant example::

    uint32_t showcase;
    
    if (showcase == 0U) {
            showcase = 32U;
    }

.. rst-class:: non-compliant-code

   Non-compliant example::

       uint32_t showcase;
       
       if(showcase == 0U){
               showcase = 32U;
       }


C-CS-07: Space shall not exist between the function identifier and the following open-parenthesis
=================================================================================================

Compliant example::

    size_t entry_size = sizeof(struct vcpuid_entry);

.. rst-class:: non-compliant-code

   Non-compliant example::

       size_t entry_size = sizeof (struct vcpuid_entry);


C-CS-08: Space shall not exist right after opening brackets and right before closing ones
=========================================================================================

Brackets in this rule refer to parenthesis, braces and squared brackets.

Compliant example::

    size_t entry_size = sizeof(struct vcpuid_entry);

.. rst-class:: non-compliant-code

   Non-compliant example::

       size_t entry_size = sizeof( struct vcpuid_entry );


C-CS-09: The '*' characters used for pointers shall be right before the function or variable identifiers
========================================================================================================

The following cases shall be covered:

a) For declaration of variables of a pointer type, the '*' character shall be
   right before the variable identifier with no space in between.
b) For functions whose return value is of a pointer type, the '*' character
   shall be right before the function identifier with no spaces in between in the
   function prototype.

Compliant example::

    uint32_t *showcase_ptr;
    uint32_t *showcase_func(void);

.. rst-class:: non-compliant-code

   Non-compliant example::

       uint32_t* showcase_ptr;
       uint32_t* showcase_func(void);


C-CS-10: A single space shall exist around binary and ternary operators
=======================================================================

This rule applies to all binary arithmetic, bit-wise, logical, relational,
equality and assignment operators, as well as the ternary conditional operator.

Compliant example::

    uint32_t showcase = 32U;
    
    showcase = showcase * 2U;

.. rst-class:: non-compliant-code

   Non-compliant example::

       uint32_t showcase=32U;
       
       showcase=showcase*2U;


C-CS-11: Space shall not exist after unary operator
===================================================

There shall be no space between a unary operator and its operand. This rule
applies to member accesses, prefix or postfix increments and decrements, address
and indirection operators.

Compliant example::

    int *x;
    int y = y + *x;
    int a = b->member;

.. rst-class:: non-compliant-code

   Non-compliant example::

       int * x;
       int y = y + * x;
       int a = b ->member;


C-CS-12: A single space shall exist right after semicolons in for-loop headers
==============================================================================

A single space shall exist right after semicolons that separate the different
expressions in for-loop headers.

Compliant example::

    uint32_t i;
    
    for (i = 0U; i < 5U; i++) {
            printf("count: %d \n", i);
    }

.. rst-class:: non-compliant-code

   Non-compliant example::

       uint32_t i;
       
       for (i = 0U;i < 5U;i++) {
               printf("count: %d \n", i);
       }


C-CS-13: Braces after if/switch/for/do/while shall be in the same line
======================================================================

The statement after if/switch/for/do/while shall always be a compound statement
with its opening brace in the same line as the keyword.

Compliant example::

    uint32_t numerator = 32U;
    uint32_t denominator = 0U;
    uint32_t quotient;
    
    if (denominator != 0U) {
            quotient = numerator / denominator;
    }

.. rst-class:: non-compliant-code

   Non-compliant example::

       uint32_t numerator = 32U;
       uint32_t denominator = 0U;
       uint32_t quotient;
       
       if (denominator != 0U)
       {
               quotient = numerator / denominator;
       }


C-CS-14: A function body shall start with a line containing a single opening brace
==================================================================================

Compliant example::

    uint32_t func_showcase(uint32_t param)
    {
            return param;
    }

.. rst-class:: non-compliant-code

   Non-compliant example::

       uint32_t func_showcase(uint32_t param) {
               return param;
       }


C-CS-15: A 'switch' statement and its subordinate 'case' shall be aligned
=========================================================================

Compliant example::

    switch(suffix) {
    case 'u':
            do_something();
            break;
    default:
            do_something_else();
            break;
    }

.. rst-class:: non-compliant-code

   Non-compliant example::

       switch(suffix) {
               case 'u':
                       do_something();
                       break;
               default:
                       do_something_else();
                       break;
       }


C-CS-16: Function parameters shall be aligned
=============================================

When function call parameters are not in single line, the parameters shall be
aligned only with tabs. Mixed-use of spaces and tabs shall not be allowed. The
number of tabs could be decided by the developers based on each case and it
shall be the same for one case.

Compliant example::

    uint32_t showcase;
    
    showcase = func(param_1,
                    param_2,
                    param_3);

.. rst-class:: non-compliant-code

   Non-compliant example::

       uint32_t showcase;
       
       showcase = func(param_1,
                  param_2,
                     param_3);


C-CS-17:  '//' shall not be used for single-line comments
=========================================================

'/\*  \*/' shall be used to replace '//' for single-line comments.

Compliant example::

    /* This is a comment */

.. rst-class:: non-compliant-code

   Non-compliant example::

       // This is a comment


C-CS-18: Function information shall be documented with doxygen-style comments
=============================================================================

Some detailed rules are listed below to illustrate the comments format for each
function:

1) The comments block shall start with '/\*\*' (slash-asterisk-asterisk) in a
   single line.
2) The comments block shall end with ' \*/' (space-asterisk-slash) in a single
   line.
3) Other than the first line and the last line, every line inside the comments
   block shall start with ' \*' (space-asterisk). It also applies to the line which
   is used to separate different paragraphs. We'll call it a blank line for
   simplicity.
4) For each function, the information shall be documented with the following
   order: brief description, detailed description, parameters description,
   pre-conditions, post-conditions, return value description, and comments
   explaining the actual return values. We'll call each block of information a
   paragraph for simplicity. A paragraph may be removed from the list if it is not
   applicable for that function.
5) Each line shall only contain the description for one parameter, or one
   pre-condition, or one post-condition, or one actual return value. We'll call
   each of these an element for simplicity.
6) A blank line shall separate different paragraphs. Inside each paragraph, a
   blank line is not required to separate each element.
7) The brief description of the function shall be documented with the format
   '@brief <brief description>'.
8) No specific format is required for the detailed description of the function.
9) The description of the function parameter shall be documented with the format
   '@param <parameter name> <parameter description>'.
10) The pre-condition of the function shall be documented with the format '@pre
    <pre-condition description>'.
11) The post-condition of the function shall be documented with the format
    '@post <post-condition description>'.
12) The brief description of the function return value shall be documented with
    the format '@return <brief description of return value>'.
13) A void-returning function shall be documented with the format '@return
    None'.
14) The comments explaining the actual return values shall be documented with
    the format '@retval <return value> <return value explanation>'.
15) If the description of one element needs to span multiple lines, each line
    shall be aligned to the start of the description in the first line for that
    element.
16) The comments block shall appear immediately before the function
    definition/declaration in the C source file or header file.

Compliant example::

    /**
     * @brief Brief description of the function.
     *
     * Detailed description of the function. Detailed description of the function. Detailed description of the
     * function. Detailed description of the function.
     *
     * @param param_1 Parameter description for param_1.
     * @param param_2 Parameter description for param_2.
     * @param param_3 Parameter description for param_3. Parameter description for param_3. Parameter description
     *                for param_3. Parameter description for param_3. Parameter description for param_3. Parameter
     *                description for param_3.
     *
     * @pre param_1 != NULL
     * @pre param_2 <= 255U
     *
     * @post retval <= 0
     *
     * @return Brief description of the return value.
     *
     * @retval 0 Success to handle specific case.
     * @retval -EINVAL Fail to handle specific case because the argument is invalid.
     * @retval -EBUSY Fail to handle specific case because the target is busy.
     *
     */
    int32_t func_showcase(uint32_t *param_1, uint32_t param_2, uint32_t param_3);

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


Naming Convention
*****************


C-NC-01: Object-like MACRO shall be named with full upper case
==============================================================

Compliant example::

    #define MAX_CONFIG_NAME_SIZE        32U

.. rst-class:: non-compliant-code

   Non-compliant example::

       #define max_config_name_size        32U


C-NC-02: Mixed-use of lower case and upper case in function-like MACRO shall not be allowed
===========================================================================================

Function-like MACRO shall be named with either full lower case or full upper
case. Mixed-use of lower case and upper case shall not be allowed.

Compliant example::

    #define max(x, y)        ((x) < (y)) ? (y) : (x)

.. rst-class:: non-compliant-code

   Non-compliant example::

       #define Max(x, y)        ((x) < (y)) ? (y) : (x)


C-NC-03: Data structures exposed to external components shall be named with prefix 'acrn'
=========================================================================================

The data structure types include struct, union, and enum.
This rule applies to the data structure with all the following properties:

a) The data structure is used by multiple modules;
b) The corresponding resource is exposed to external components, such as SOS or
   UOS;
c) The name meaning is simplistic or common, such as vcpu or vm.

Compliant example::

    struct acrn_vcpu {
            ...
    };

.. rst-class:: non-compliant-code

   Non-compliant example::

       struct vcpu {
               ...
       };


C-NC-04: Data structures only used by hypervisor shall be named with prefix 'hv'
================================================================================

The data structure types include struct, union, and enum.
This rule applies to the data structure with all the following properties:

a) The data structure is used by multiple modules;
b) The corresponding resource is only used by hypervisor;
c) The name meaning is simplistic or common, such as timer.

Compliant example::

    struct hv_timer {
            ...
    };

.. rst-class:: non-compliant-code

   Non-compliant example::

       struct timer {
               ...
       };


C-NC-05: Data structures only used by one module shall be named with the module name as prefix
==============================================================================================

The data structure types include struct, union, and enum.
This rule applies to the data structure with all the following properties:

a) The data structure is only used by one module;
b) The name meaning is simplistic or common, such as context.

Compliant example::

    struct instr_emul_ctxt {
            ...
    };

.. rst-class:: non-compliant-code

   Non-compliant example::

       struct ctxt {
               ...
       };


C-NC-06: Data structures related to hardware resource shall be named with the resource name as suffix
=====================================================================================================

The data structure types include struct, union, and enum.
For example:

a) The data structure related to register shall be named with suffix 'reg';
b) The data structure related to segment selector shall be named with suffix
   'sel'.

Compliant example::

    struct lapic_reg {
            ...
    };

.. rst-class:: non-compliant-code

   Non-compliant example::

       struct lapic {
               ...
       };


C-NC-07: Function pointer shall be named with suffix 'fn'
=========================================================

Compliant example::

    struct firmware_operations {
            void (*init_fn)(void);
            void *(*get_rsdp_fn)(void);
    };

.. rst-class:: non-compliant-code

   Non-compliant example::

       struct firmware_operations {
               void (*init)(void);
               void *(*get_rsdp)(void);
       };


Implementation-specific Behaviors
*********************************


C-IB-01: All characters in an identifier are significant initial characters
===========================================================================

The number of significant initial characters in an identifier is
implementation-defined, according to J.3.3 item 2 in C99. For ACRN hypervisor,
all characters in an identifier are significant initial characters.

C-IB-02: The number of bits in a byte is 8
==========================================

The number of bits in a byte is implementation-defined, according to J.3.4 item
1 in C99. For ACRN hypervisor, the number of bits in a byte is 8.

C-IB-03: The values of the members of the execution character set depends on ASCII Table
========================================================================================

The values of the members of the execution character set is
implementation-defined, according to J.3.4 item 2 in C99. For ACRN hypervisor,
characters are encoded in ASCII. This rule applies to the source code that is
being compiled. Non-ASCII characters are allowed in comments, such as the author
name.

C-IB-04:  'plain' char is equivalent to signed char
===================================================

The underlying type of 'plain' char is implementation-defined, according to
J.3.4 item 5 in C99. For ACRN hypervisor, 'plain' char is equivalent to signed
char.

C-IB-05: Signed integers are represented in two's complement
============================================================

Whether signed integer types are represented using sign and magnitude, two's
complement, or ones' complement is implementation-defined, according to J.3.5
item 2 in C99. For ACRN hypervisor, signed integers are represented in two's
complement.

C-IB-06: The integer type compatible with each enumerated type is case by case
==============================================================================

The integer type compatible with each enumerated type is implementation-defined,
according to J.3.9 item 6 in C99. For ACRN hypervisor, if the enum has no
negative underlying values, unsigned int is used; otherwise, int is used.

C-IB-07: The number of bytes in an object is specified
======================================================

The number of bytes in an object is implementation-defined, according to J.3.13
item 2 in C99. For ACRN hypervisor, char is 1 byte, short is 2 bytes, int is 4
bytes, long is 8 bytes, and long long is not used.

Language Extensions
*******************


C-LE-01: Use of inline Assembly language in C Code is allowed
=============================================================

This feature refers to section 6.45 in GCC 7.3 Manual.

C-LE-02: Use of builtin-type '__builtin_va_list' is allowed
===========================================================

This feature refers to section 6.20 in GCC 7.3 Manual.

C-LE-03: Use of extended type attribute is allowed
==================================================

This rule applies to the following type attributes:

a) 'aligned', refers to section 6.33.1 in GCC 7.3 Manual.
b) 'packed', refers to section 6.33.1 in GCC 7.3 Manual.
c) 'unused', refers to section 6.33.1 in GCC 7.3 Manual.
d) 'section', refers to section 6.32.1 in GCC 7.3 Manual.

C-LE-04: Use of extended builtin-function is allowed
====================================================

This rule applies to the following builtin-functions:

a) '__builtin_va_arg', refers to section 6.20 in GCC 7.3 Manual.
b) '__builtin_va_start', refers to section 6.20 in GCC 7.3 Manual.
c) '__builtin_va_end', refers to section 6.20 in GCC 7.3 Manual.
d) '__builtin_offsetof', refers to section 6.51 in GCC 7.3 Manual.

