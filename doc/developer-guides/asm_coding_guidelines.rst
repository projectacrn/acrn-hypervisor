.. _asm_coding_guidelines:

Assembly Language Coding Guidelines
###################################

.. contents::
   :local:


General
*******

ASM-GN-01: One address shall not be declared by two labels
==========================================================

Compliant example::

    asm_showcase_1:
            movl    $0x1, %eax

.. rst-class:: non-compliant-code

   Non-compliant example::

       asm_showcase_1:
       asm_showcase_2:
               movl    $0x1, %eax


ASM-GN-02: Names reserved for use by the assembler shall not be used for any other purpose
==========================================================================================

Names defined by developers shall not be the same as an assembler directive,
instruction prefix, instruction mnemonic, or register name.

Compliant example::

    asm_showcase_1:
            movl    $0x1, %eax
    
    asm_showcase_2:
            movl    $0x2, %eax
    
    asm_showcase_3:
            movl    $0x3, %eax

.. rst-class:: non-compliant-code

   Non-compliant example::

       text:
               movl    $0x1, %eax
       
       mov:
               movl    $0x2, %eax
       
       eax:
               movl    $0x3, %eax


ASM-GN-03: All declared labels shall be used
============================================

Otherwise, the label shall be removed.

Compliant example::

    asm_showcase_1:
            movl    $0x1, %eax
            jmp     asm_showcase_2
    
    /* do something */
    
    asm_showcase_2:
            movl    $0x2, %eax

.. rst-class:: non-compliant-code

   Non-compliant example::

       asm_showcase_1:
               movl    $0x1, %eax
       
       /*
        * 'asm_showcase_2' is not used anywhere, including
        *  all C source/header files and Assembly files.
        */
       asm_showcase_2:
               movl    $0x2, %eax


ASM-GN-04: Magic numbers shall be used with restrictions
========================================================

Only the following cases shall be allowed:

a) The magic number is defined as a MACRO with a name clearly indicating its
   meaning.
b) The meaning of the magic number is clearly documented in the comments before
   its usage.
c) The meaning of the magic number is straightforward in the specific context.

Compliant example::

    .section .data
    showcase_data:
            /* 0xff000000 means <something> */
            .long   0xff000000

.. rst-class:: non-compliant-code

   Non-compliant example::

       .section .data
       showcase_data:
               .long   0xff000000


ASM-GN-05: Parentheses shall be used to set the operator precedence explicitly
==============================================================================

Compliant example::

    .section .data
    showcase_data:
            /* 0x1234 means <something> */
            .long   0x1234 * (0x1234 >> 2)

.. rst-class:: non-compliant-code

   Non-compliant example::

       .section .data
       showcase_data:
               /* 0x1234 means <something> */
               .long   0x1234 * 0x1234 >> 2


ASM-GN-06: .end directive statement shall be the last statement in an Assembly file
===================================================================================

This rule only applies to the Assembly file which uses .end directive. .end
directive shall be the last statement in this case. All the statements past .end
directive will not be processed by the assembler.

Compliant example::

    #include <types.h>
    #include <spinlock.h>
    
    .macro asm_showcase_mov
            movl    $0x1, %eax
    .endm
    
    .end

.. rst-class:: non-compliant-code

   Non-compliant example::

       #include <types.h>
       
       .end
       
       #include <spinlock.h>
       
       .macro asm_showcase_mov
               movl    $0x1, %eax
       .endm


ASM-GN-07: Infinite loop shall not exist
========================================

Compliant example::

    asm_showcase_1:
            movl    $0x1, %eax
            jmp     asm_showcase_2
    
    /* do something */
    
    asm_showcase_2:
            movl    $0x2, %eax

.. rst-class:: non-compliant-code

   Non-compliant example::

       asm_showcase_1:
               movl    $0x1, %eax
               jmp     asm_showcase_1


ASM-GN-08: All code shall be reachable
======================================

Compliant example::

    asm_showcase:
            movl    %ebx, %eax
            test    $0x400, %eax
            jne     asm_test
            movl    $0x2, %eax
            movl    $0x3, %eax
    
    asm_test:
            movl    $0x6, %eax

.. rst-class:: non-compliant-code

   Non-compliant example::

       asm_showcase:
               movl    %ebx, %eax
               jmp     asm_test
               /* the following two lines have no chance to be executed */
               movl    $0x2, %eax
               movl    $0x3, %eax
       
       asm_test:
               movl    $0x6, %eax


ASM-GN-09: Far jump shall be used with restrictions
===================================================

Jumping to an instruction located in a different segment shall only be used for
the following two cases:

a) Code bit changes, such as change from 32-bit mode to 64-bit mode.
b) System resumes from S3. In this case, Global Descriptor Table (GDT) is set by
   Bootloader/BIOS and far jump has to be used to correct the Code Segment (CS).

Compliant example::

    .code32
    execution_32:
            /*
             * do something in 32-bit mode,
             * then,
             * perform a far jump to start executing in 64-bit mode
             */
            ljmp    $0x0008, $execution_64_2
    
    .code64
    execution_64_1:
            /* do something in 64-bit mode */
    
    execution_64_2:
            /* do something in 64-bit mode */

.. rst-class:: non-compliant-code

   Non-compliant example::

       .data
       asm_showcase_data:
               .word   0x0008
       
       .code32
       execution_32:
               /* do something in 32-bit mode */
               ljmp    $0x0008, $asm_showcase_data


ASM-GN-10: Assembler directives shall be used with restrictions
===============================================================

Usage of the assembler directive refers to GNU assembler 'as' user manual. Only
the following assembler directives may be used:

1) .align
2) .end
3) .extern
4) repeat related directives, including .rept and .endr
5) global related directives, including .global and .globl
6) macro related directives, including .altmacro, .macro, and .endm
7) code bit related directives, including .code16, .code32, and .code64
8) section related directives, including .section, .data, and .text
9) number emission related directives, including .byte, .word, .short, .long,
   and .quad
10) .org, which shall be used with restrictions. It shall only be used to
    advance the location counter due to code bit changes, such as change from 32-bit
    mode to 64-bit mode.



Functions
*********

ASM-FN-01: Function shall have return statement
===============================================

Compliant example::

    asm_func_showcase:
            movl    $0x2, %eax
            ret
    
    asm_showcase:
            movl    $0x1, %eax
            call    asm_func_showcase

.. rst-class:: non-compliant-code

   Non-compliant example::

       asm_func_showcase:
               movl    $0x2, %eax
       
       asm_showcase:
               movl    $0x1, %eax
               call    asm_func_showcase


ASM-FN-02: A function shall only have one entry point
=====================================================

The label in a function shall only be used inside. Jumping into the function
from outside via this label shall not be allowed. This rule applies to both
conditional jump and unconditional jump.

Compliant example::

    asm_func_showcase:
            test    $0x400, %eax
            jne     tmp
            movl    $0x1, %eax
    tmp:
            movl    $0x2, %eax
            ret
    
    asm_showcase:
            movl    $0x1, %eax
            call    asm_func_showcase

.. rst-class:: non-compliant-code

   Non-compliant example::

       asm_func_showcase:
               movl    $0x1, %eax
       tmp:
               movl    $0x2, %eax
               ret
       
       asm_showcase:
               movl    $0x1, %eax
               call    asm_func_showcase
               jmp     tmp


ASM-FN-03: A function shall only have one return statement
==========================================================

Compliant example::

    asm_func_showcase:
            test    $0x400, %eax
            jne     tmp
            movl    $0x2, %eax
            jmp     showcase_return
    tmp:
            movl    $0x3, %eax
    showcase_return:
            ret

.. rst-class:: non-compliant-code

   Non-compliant example::

       asm_func_showcase:
               test    $0x400, %eax
               jne     tmp
               movl    $0x2, %eax
               ret
       tmp:
               movl    $0x3, %eax
               ret


ASM-FN-04: Function shall only be entered by explicit call
==========================================================

Falling through from prior instruction shall not be allowed.

Compliant example::

    asm_func_showcase:
            movl    $0x2, %eax
            ret
    
    asm_showcase:
            movl    $0x1, %eax
            call    asm_func_showcase

.. rst-class:: non-compliant-code

   Non-compliant example::

       asm_showcase:
               movl    $0x1, %eax
       
       asm_func_showcase:
               movl    $0x2, %eax
               ret


ASM-FN-05: A jump instruction shall not be used to jump out of a function
=========================================================================

This rule applies to both conditional jump and unconditional jump.

Compliant example::

    asm_func_showcase:
            movl    $0x2, %eax
            ret
    
    asm_showcase:
            movl    $0x1, %eax
            call    asm_func_showcase

.. rst-class:: non-compliant-code

   Non-compliant example::

       asm_func_showcase:
               movl    $0x2, %eax
               jmp     asm_test
               ret
       
       asm_showcase:
               movl    $0x1, %ebx
               call    asm_func_showcase
       
       asm_test:
               cli


ASM-FN-06: Recursion shall not be used in function calls
========================================================

Compliant example::

    asm_func_showcase:
            movl    $0x2, %eax
            ret
    
    asm_showcase:
            movl    $0x1, %eax
            call    asm_func_showcase

.. rst-class:: non-compliant-code

   Non-compliant example::

       asm_func_showcase:
               movl    $0x2, %eax
               call    asm_func_showcase
               ret
       
       asm_showcase:
               movl    $0x1, %eax
               call    asm_func_showcase


ASM-FN-07: Cyclomatic complexity shall be less than 10
======================================================

A function with cyclomatic complexity greater than 10 shall be split into
multiple sub-functions to simplify the function logic.

Compliant example::

    asm_func_showcase:
            /* do something */
            cmpl    $0x0, %eax
            je      tmp
            cmpl    $0x1, %eax
            je      tmp
            cmpl    $0x2, %eax
            je      tmp
            /* do something */
    tmp:
            /* do something */
            ret

.. rst-class:: non-compliant-code

   Non-compliant example::

       asm_func_showcase:
               /* do something */
               cmpl    $0x0, %eax
               je      tmp
               cmpl    $0x1, %eax
               je      tmp
               cmpl    $0x2, %eax
               je      tmp
               cmpl    $0x3, %eax
               je      tmp
               cmpl    $0x4, %eax
               je      tmp
               cmpl    $0x5, %eax
               je      tmp
               cmpl    $0x6, %eax
               je      tmp
               cmpl    $0x7, %eax
               je      tmp
               cmpl    $0x8, %eax
               je      tmp
               cmpl    $0x9, %eax
               je      tmp
               cmpl    $0xa, %eax
               je      tmp
               cmpl    $0xb, %eax
               je      tmp
               cmpl    $0xc, %eax
               je      tmp
               cmpl    $0xd, %eax
               je      tmp
               cmpl    $0xe, %eax
               je      tmp
               /* do something */
       tmp:
               /* do something */
               ret



Coding Style
************

ASM-CS-01: One instruction statement shall not be split into multiple lines
===========================================================================

Compliant example::

    movl    $0x2, %eax

.. rst-class:: non-compliant-code

   Non-compliant example::

       movl    $0x2, \
       %eax


ASM-CS-02: Assembler directive statements shall be aligned
==========================================================

An assembler directive statement is composed of directive and arguments.
Arguments are optional depending on the use case.
Some detailed rules about the alignment are listed below:

a) Assembler directives shall be aligned with one tab if the statement is in the
   code block under any label from functional perspective. Otherwise, assembler
   directives shall be aligned to the start of the line.
b) Tabs shall be used to separate the directive and the first argument, where
   applicable. The number of tabs could be decided by the developers based on each
   case and it shall guarantee that the first argument is aligned in each directive
   block.
c) A single space shall be used to separate multiple arguments.

Compliant example::

    .extern         cpu_primary_save32
    .extern         cpu_primary_save64
    
    .section        multiboot_header, "a"
    .align          4
    .long           0x0008
    .long           0x0018
    
    .section        entry, "ax"
    .align          8
    .code32

.. rst-class:: non-compliant-code

   Non-compliant example::

          .extern      cpu_primary_save32
          .extern   cpu_primary_save64
       
       .section     multiboot_header, "a"
       .align  4
       .long     0x0008
       .long   0x0018
       
          .section   entry, "ax"
          .align   8
         .code32


ASM-CS-03: Labels shall be aligned to the start of the line
===========================================================

Compliant example::

    asm_showcase_1:
            movl    $0x1, %eax
    
    asm_showcase_2:
            movl    $0x2, %eax

.. rst-class:: non-compliant-code

   Non-compliant example::

         asm_showcase_1:
            movl    $0x1, %eax
       
          asm_showcase_2:
            movl    $0x2, %eax


ASM-CS-04: Instruction statements shall be aligned
==================================================

An instruction statement is composed of instruction prefix, instruction
mnemonic, and instruction operands. Instruction prefix and instruction operands
are optional depending on the use case.
Some detailed rules about the alignment are listed below:

a) The start of instruction statements shall be aligned with one tab if the
   instruction statement is in the code block under any label from functional
   perspective. Otherwise, the start of instruction statements shall be aligned to
   the start of the line. The start of the instruction could either be the
   instruction mnemonic or the instruction prefix.
b) One space shall be used to separate the instruction prefix and the
   instruction mnemonic, where applicable.
c) Tabs shall be used to separate the instruction mnemonic and the first
   instruction operand, where applicable. The number of tabs could be decided by
   the developers based on each case and it shall guarantee that the first
   instruction operand in the code block under one label is aligned.
d) A single space shall be used to separate multiple operands.

Compliant example::

    asm_showcase_1:
            movl            $0x1, %eax
            lock and        %rcx, (%rdx)
    
    asm_showcase_2:
            movl            $0x3, %eax

.. rst-class:: non-compliant-code

   Non-compliant example::

       asm_showcase_1:
       movl   $0x1, %eax
         lock    and        %rcx, (%rdx)
       
       asm_showcase_2:
           movl     $0x2, %eax


ASM-CS-05:  '//' shall not be used for comments
===============================================

'/* \*/' shall be used to replace '//' for comments.

Compliant example::

    /* This is a comment */
    movl    $0x1, %eax

.. rst-class:: non-compliant-code

   Non-compliant example::

       // This is a comment
       movl    $0x1, %eax


ASM-CS-06: Tabs shall be 8 characters wide
==========================================

A tab character shall be considered 8-character wide when limiting the line
width.


ASM-CS-07: Each line shall contain at most 120 characters
=========================================================

No more than 120 characters shall be on a line, with tab stops every 8
characters.

Compliant example::

    /*
     * This is a comment. This is a comment. This is a comment. This is a comment.
     * This is a comment. This is a comment. This is a comment.
     */

.. rst-class:: non-compliant-code

   Non-compliant example::

       /* This is a comment. This is a comment. This is a comment. This is a comment. This is a comment. This is a comment. This is a comment. */


ASM-CS-08: Legal entity shall be documented in every file
=========================================================

Legal entity shall be documented in a separate comments block at the start of
every file.
The following information shall be included:

a) Copyright
b) License (using an `SPDX-License-Identifier <https://spdx.org/licenses/>`_)

Compliant example::

    /* Legal entity shall be placed at the start of the file. */
    -------------File Contents Start After This Line------------
    
    /*
     * Copyright (C) 2019 Intel Corporation.
     *
     * SPDX-License-Identifier: BSD-3-Clause
     */
    
    /* Coding or implementation related comments start after the legal entity. */
    .code64

.. rst-class:: non-compliant-code

   Non-compliant example::

       /* Neither copyright nor license information is included in the file. */
       -------------------File Contents Start After This Line------------------
       
       /* Coding or implementation related comments start directly. */
       .code64



Naming Convention
*****************

ASM-NC-01: Lower case letters shall be used for case insensitive names
======================================================================

This rule applies to assembler directive, instruction prefix, instruction
mnemonic, and register name.

Compliant example::

    .code64
    lock and        %rcx, (%rdx)

.. rst-class:: non-compliant-code

   Non-compliant example::

       .CODE64
       LOCK AND        %RCX, (%RDX)


ASM-NC-02: Names defined by developers shall use lower case letters
===================================================================

Names defined by developers shall use lower case letters with the following
exception. If an object-like MACRO is defined with '#define', it shall be named
with full upper case.

Compliant example::

    asm_showcase:
            movl    $0x1, %eax

.. rst-class:: non-compliant-code

   Non-compliant example::

       ASM_SHOWCASE:
               movl    $0x1, %eax


ASM-NC-03: Label name shall be unique
=====================================

Label name shall be unique with the following exception. Usage of local labels
is allowed. Local label is defined with the format 'N:', where N represents any
non-negative integer. Using 'Nb' to refer to the most recent previous definition
of that label. Using 'Nf' to refer to the next definition of a local label.

Compliant example::

    asm_showcase_1:
            movl    $0x1, %eax
    
    asm_showcase_2:
            movl    $0x2, %eax

.. rst-class:: non-compliant-code

   Non-compliant example::

       asm_showcase:
               movl    $0x1, %eax
       
       asm_showcase:
               movl    $0x2, %eax


ASM-NC-04: Names defined by developers shall be less than 31 characters
=======================================================================

Compliant example::

    asm_showcase:
            movl    $0x1, %eax

.. rst-class:: non-compliant-code

   Non-compliant example::

       asm_showcase_asm_showcase_asm_showcase:
               movl    $0x1, %eax


