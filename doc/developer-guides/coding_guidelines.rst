.. _coding_guidelines:

Coding Guidelines
#################

.. contents::
   :local:


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



Identifiers
***********

ID-01: static keyword shall not be used in an array index declaration
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


