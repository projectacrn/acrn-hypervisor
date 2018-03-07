/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <vmx.h>
#include <msr.h>
#include <guest.h>
#include <vcpu.h>
#include <cpu.h>
#include <types.h>

    .text

/*int vmx_vmrun(struct run_context *context, int launch, int ibrs_type) */
   .code64
   .align       8
   .global      vmx_vmrun
vmx_vmrun:

    /* Save all host GPRs that must be preserved across function calls
       per System V ABI */
    push        %rdx
    push        %rbx
    push        %rbp
    push        %r12
    push        %r13
    push        %r14
    push        %r15

    /* Save RDI on top of host stack for easy access to VCPU pointer
       on return from guest context */
    push        %rdi

    /* rdx = ibrs_type */
    /* if ibrs_type != IBRS_NONE, means IBRS feature is supported,
     * restore MSR SPEC_CTRL to guest
     */
    cmp         $IBRS_NONE,%rdx
    je          next

    movl        $MSR_IA32_SPEC_CTRL,%ecx
    mov         VMX_MACHINE_T_GUEST_SPEC_CTRL_OFFSET(%rdi),%rax
    movl        $0,%edx
    wrmsr

next:

    /* Load VMCS_HOST_RSP_FIELD field value */
    mov         $VMX_HOST_RSP,%rdx

    /* Write the current stack pointer to the VMCS_HOST_RSP_FIELD */
    vmwrite     %rsp,%rdx

    /* Error occurred - handle error */
    jbe         vm_eval_error

    /* Compare the launch flag to see if launching (1) or resuming (0) */
    cmp         $VM_LAUNCH, %rsi

    mov         VMX_MACHINE_T_GUEST_CR2_OFFSET(%rdi),%rax
    mov         %rax,%cr2

    mov         VMX_MACHINE_T_GUEST_RAX_OFFSET(%rdi),%rax
    mov         VMX_MACHINE_T_GUEST_RBX_OFFSET(%rdi),%rbx
    mov         VMX_MACHINE_T_GUEST_RCX_OFFSET(%rdi),%rcx
    mov         VMX_MACHINE_T_GUEST_RDX_OFFSET(%rdi),%rdx
    mov         VMX_MACHINE_T_GUEST_RBP_OFFSET(%rdi),%rbp
    mov         VMX_MACHINE_T_GUEST_RSI_OFFSET(%rdi),%rsi
    mov         VMX_MACHINE_T_GUEST_R8_OFFSET(%rdi),%r8
    mov         VMX_MACHINE_T_GUEST_R9_OFFSET(%rdi),%r9
    mov         VMX_MACHINE_T_GUEST_R10_OFFSET(%rdi),%r10
    mov         VMX_MACHINE_T_GUEST_R11_OFFSET(%rdi),%r11
    mov         VMX_MACHINE_T_GUEST_R12_OFFSET(%rdi),%r12
    mov         VMX_MACHINE_T_GUEST_R13_OFFSET(%rdi),%r13
    mov         VMX_MACHINE_T_GUEST_R14_OFFSET(%rdi),%r14
    mov         VMX_MACHINE_T_GUEST_R15_OFFSET(%rdi),%r15

    mov         VMX_MACHINE_T_GUEST_RDI_OFFSET(%rdi),%rdi

    /* Execute appropriate VMX instruction */
    je          vm_launch

    /* Execute a VM resume */
    vmresume

vm_launch:

    /* Execute a VM launch */
    vmlaunch

    .global vm_exit
vm_exit:

    /* Get VCPU data structure pointer from top of host stack and
       save guest RDI in its place */
    xchg        0(%rsp),%rdi

    /* Save current GPRs to guest state area */
    mov         %rax,VMX_MACHINE_T_GUEST_RAX_OFFSET(%rdi)

    mov         %cr2,%rax
    mov         %rax,VMX_MACHINE_T_GUEST_CR2_OFFSET(%rdi)

    mov         %rbx,VMX_MACHINE_T_GUEST_RBX_OFFSET(%rdi)
    mov         %rcx,VMX_MACHINE_T_GUEST_RCX_OFFSET(%rdi)
    mov         %rdx,VMX_MACHINE_T_GUEST_RDX_OFFSET(%rdi)
    mov         %rbp,VMX_MACHINE_T_GUEST_RBP_OFFSET(%rdi)
    mov         %rsi,VMX_MACHINE_T_GUEST_RSI_OFFSET(%rdi)
    mov         %r8,VMX_MACHINE_T_GUEST_R8_OFFSET(%rdi)
    mov         %r9,VMX_MACHINE_T_GUEST_R9_OFFSET(%rdi)
    mov         %r10,VMX_MACHINE_T_GUEST_R10_OFFSET(%rdi)
    mov         %r11,VMX_MACHINE_T_GUEST_R11_OFFSET(%rdi)
    mov         %r12,VMX_MACHINE_T_GUEST_R12_OFFSET(%rdi)
    mov         %r13,VMX_MACHINE_T_GUEST_R13_OFFSET(%rdi)
    mov         %r14,VMX_MACHINE_T_GUEST_R14_OFFSET(%rdi)
    mov         %r15,VMX_MACHINE_T_GUEST_R15_OFFSET(%rdi)

    /* Load guest RDI off host stack and into RDX */
    mov         0(%rsp),%rdx

    /* Save guest RDI to guest state area */
    mov         %rdx,VMX_MACHINE_T_GUEST_RDI_OFFSET(%rdi)

    /* Save RDI to RSI for later SPEC_CTRL save*/
    mov         %rdi,%rsi

vm_eval_error:

    /* Restore host GPR System V required registers */
    pop         %rdi
    pop         %r15
    pop         %r14
    pop         %r13
    pop         %r12
    pop         %rbp
    pop         %rbx
    pop         %rdx


    /* Check vm fail, refer to 64-ia32 spec section 26.2 in volume#3 */
    mov         $VM_FAIL,%rax
    jc          vm_return
    jz          vm_return

    /* Clear host registers to prevent speculative use */
    xor         %rcx,%rcx
    xor         %r8,%r8
    xor         %r9,%r9
    xor         %r10,%r10
    xor         %r11,%r11

    /* rdx = ibrs_type */
    /* IBRS_NONE: no ibrs setting, just flush rsb
     * IBRS_RAW: set IBRS then flush rsb
     * IBRS_OPT: set STIBP & IBPB then flush rsb
     */
    cmp         $IBRS_NONE,%rdx
    je          stuff_rsb

    cmp         $IBRS_OPT,%rdx
    je          ibrs_opt

    /* Save guest MSR SPEC_CTRL, low 32 bit is enough */
    movl        $MSR_IA32_SPEC_CTRL,%ecx
    rdmsr
    mov         %rax,VMX_MACHINE_T_GUEST_SPEC_CTRL_OFFSET(%rsi)
    movl        $SPEC_ENABLE_IBRS,%eax
    movl        $0,%edx
    wrmsr

    jmp         stuff_rsb

ibrs_opt:

    movl        $MSR_IA32_PRED_CMD,%ecx
    movl        $PRED_SET_IBPB,%eax
    movl        $0,%edx
    wrmsr

    /* Save guest MSR SPEC_CTRL, low 32 bit is enough */
    movl        $MSR_IA32_SPEC_CTRL,%ecx
    rdmsr
    mov         %rax,VMX_MACHINE_T_GUEST_SPEC_CTRL_OFFSET(%rsi)
    movl        $SPEC_ENABLE_STIBP,%eax
    movl        $0,%edx
    wrmsr

    /* stuff rsb by 32 CALLs, make sure no any "ret" is executed before this
     * stuffing rsb, otherwise, someone may insert some code before this for
     * future update.
     */
stuff_rsb:

    /* stuff 32 RSB, rax = 32/2 */
    mov         $16,%rax
.align 16
3:
    call        4f
33:
    pause
    jmp         33b
.align 16
4:
    call        5f
44:
    pause
    jmp         44b
.align 16
5:  dec         %rax
    jnz         3b
    /* stuff 32 RSB, rsp += 8*32 */
    add         $(8*32),%rsp

    mov         $VM_SUCCESS,%rax

vm_return:
    /* Return to caller */
    ret

