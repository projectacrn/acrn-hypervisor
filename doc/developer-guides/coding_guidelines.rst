.. _coding_guidelines:

Coding Guidelines
#################

.. contents::
   :local:


Preprocessor
************

PP-01: ## or # operators shall be used with restrictions
========================================================

## or # operators shall only be used alone. The following cases shall not be
allowed:

a) The result getting from ## or # operation shall not be used as the operands
   of another ## or # operation;
b) Mixed-use of ## or # operators shall not be allowed.

Compliant example::

    #define CONCAT(x, y) x ## y
    
    uint32_t ab = 32;
    printf("%d \n", CONCAT(a, b));

.. rst-class:: non-compliant-code

   Non-compliant example::

       #define CONCAT(x, y, z) x ## y ## z
       
       uint32_t abc = 32;
       printf("%d \n", CONCAT(a, b, c));



Compilation Units
*****************

CU-01: Only one assignment shall be on a single line
====================================================

Multiple assignments on a single line are not allowed.

Compliant example::

    a = d;
    b = d;
    c = d;

.. rst-class:: non-compliant-code

   Non-compliant example::

       int a = b = c = d;


CU-02: Only one return statement shall be in a function
=======================================================

Multiple return statements in a function are not allowed.

Compliant example::

    int32_t foo(char *ptr) {
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


CU-03: All code shall be reachable
==================================

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


CU-04: Cyclomatic complexity shall be less than 20
==================================================

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

DI-01: Variable shall be used after its initialization
======================================================

Compliant example::

    uint32_t a, b;
    
    a = 0U;
    b = a;

.. rst-class:: non-compliant-code

   Non-compliant example::

       uint32_t a, b;
       
       b = a;


DI-02: Function shall be called after its declaration
=====================================================

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


DI-03: The initialization statement shall not be skipped
========================================================

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


DI-04: The initialization of struct shall be enclosed with brackets
===================================================================

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


DI-05: The array size shall be specified explicitly
===================================================

Compliant example::

    uint32_t showcase[2] = {0U, 1U};

.. rst-class:: non-compliant-code

   Non-compliant example::

       uint32_t showcase[] = {0U, 1U};


DI-06: Global variables shall only be declared once
===================================================

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



Functions
*********

FN-01: A non-void function shall have return statement
======================================================

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


FN-02: A non-void function shall have return value rather than empty return
===========================================================================

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


FN-03: A non-void function shall return value on all paths
==========================================================

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


FN-04: The return value of a void-returning function shall not be used
======================================================================

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


FN-05: Parameter passed by pointer to a function shall not be reassigned
========================================================================

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


FN-06: Parameter passed by value to a function shall not be modified directly
=============================================================================

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


FN-07: Non-static function shall be declared in header file
===========================================================

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



Statements
**********

ST-01: sizeof operator shall not be performed on an array function parameter
============================================================================

When an array is used as the function parameter, the array address is passed.
Thus, the return value of the sizeof operation is the pointer size rather than
the array size.

Compliant example::

    #define SHOWCASE_SIZE 32U
    
    void showcase(uint32_t array_source[SHOWCASE_SIZE]) {
            uint32_t num_bytes = SHOWCASE_SIZE * sizeof(uint32_t);
    
            printf("num_bytes %d \n", num_bytes);
    }

.. rst-class:: non-compliant-code

   Non-compliant example::

       #define SHOWCASE_SIZE 32U
       
       void showcase(uint32_t array_source[SHOWCASE_SIZE]) {
           uint32_t num_bytes = sizeof(array_source);
       
           printf("num_bytes %d \n", num_bytes);
       }


ST-02: Argument of strlen shall end with a null character
=========================================================

Compliant example::

    uint32_t size;
    char showcase[3] = {'0', '1', '\0'};
    
    size = strlen(showcase);

.. rst-class:: non-compliant-code

   Non-compliant example::

       uint32_t size;
       char showcase[2] = {'0', '1'};
       
       size = strlen(showcase);


ST-03: Two strings shall not be copied to each other if they have memory overlap
================================================================================

Compliant example::

    char *str_source = "showcase";
    char str_destination[32];
    
    (void)strncpy(str_destination, str_source, 8U);

.. rst-class:: non-compliant-code

   Non-compliant example::

       char *str_source = "showcase";
       char *str_destination = &str_source[1];
       
       (void)strncpy(str_destination, str_source, 8U);


ST-04: memcpy shall not be performed on objects with overlapping memory
=======================================================================

Compliant example::

    char *str_source = "showcase";
    char str_destination[32];
    
    (void)memcpy(str_destination, str_source, 8U);

.. rst-class:: non-compliant-code

   Non-compliant example::

       char str_source[32];
       char *str_destination = &str_source[1];
       
       (void)memcpy(str_destination, str_source, 8U);


ST-05: Assignment shall not be performed between variables with overlapping storage
===================================================================================

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


ST-06: The array size shall be valid if the array is function input parameter
=============================================================================

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


ST-07: The destination object shall have sufficient space for operation
=======================================================================

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


ST-08: The size param to memcpy/memset shall be valid
=====================================================

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


ST-09: The denominator of a divide shall not be zero
====================================================

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


ST-10: A NULL pointer shall not be dereferenced
===============================================

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


ST-11: The condition of a selection or iteration statement shall not be constant
================================================================================

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


ST-12: A string literal shall not be modified
=============================================

Compliant example::

    const char *showcase = "showcase";
    
    printf("%s \n", showcase);

.. rst-class:: non-compliant-code

   Non-compliant example::

       char *showcase = "showcase";
       
       showcase[0] = 'S';
       printf("%s \n", showcase);


ST-13: The loop body shall be enclosed with brackets
====================================================

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


ST-14: A space shall exist between the expression and the control keywords
==========================================================================

A space shall exist between the expression and the control keywords, including
if, switch, while, and for.

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


ST-15: A space shall exist after the semicolon in a for expression
==================================================================

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


ST-16: A space shall exist around binary operators and assignment operators
===========================================================================

Compliant example::

    uint32_t showcase = 32U;
    
    showcase = showcase * 2U;

.. rst-class:: non-compliant-code

   Non-compliant example::

       uint32_t showcase=32U;
       
       showcase=showcase*2U;



Types
*****

TY-01: The function return value shall be consistent with the declared return type
==================================================================================

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


TY-02: The operands of bit operations shall be unsigned
=======================================================

Compliant example::

    uint32_t showcase = 32U;
    uint32_t mask = 0xFU;
    
    showcase = showcase & mask;

.. rst-class:: non-compliant-code

   Non-compliant example::

       uint32_t showcase = 32U;
       int32_t mask = -1;
       
       showcase = showcase & mask;


TY-03: Mixed-use between Boolean values and integers shall not be allowed
=========================================================================

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


TY-04: The enum shall not be used for arithmetic operations
===========================================================

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


TY-05: static keyword shall not be used in an array index declaration
=====================================================================

Compliant example::

    char showcase[2] = {'0', '1'};
    char chr;
    
    chr = showcase[1];

.. rst-class:: non-compliant-code

   Non-compliant example::

       char showcase[2] = {'0', '1'};
       char chr;
       
       chr = showcase[static 1];


TY-06: A pointer shall point to const object if the object is not modified
==========================================================================

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


TY-07: The expressions type of ternary operation shall be consistent
====================================================================

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


TY-08: The struct field type shall be consistent
================================================

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



Identifiers
***********

ID-01: A parameter name shall not be the same as the name of struct, union, enum, variable, or function
=======================================================================================================

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


ID-02: A member name shall not be the same as the name of struct, union, or enum
================================================================================

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


ID-03: A global variable name shall not be the same as the name of struct, union, or enum
=========================================================================================

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


ID-04: A local variable name shall not be the same as a global variable name
============================================================================

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


ID-05: The function name shall be unique
========================================

The function name shall not be the same as the name of struct, union, enum,
variable, or other functions.

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


