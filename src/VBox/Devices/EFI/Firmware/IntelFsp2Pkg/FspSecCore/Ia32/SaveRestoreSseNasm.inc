;------------------------------------------------------------------------------
;
; Copyright (c) 2015 - 2022, Intel Corporation. All rights reserved.<BR>
; SPDX-License-Identifier: BSD-2-Clause-Patent
;
; Abstract:
;
;   Provide macro for register save/restore using SSE registers
;
;------------------------------------------------------------------------------

;
; Define SSE instruction set
;
%ifdef USE_SSE41_FLAG
;
; Define SSE macros using SSE 4.1 instructions
; args 1:XMM, 2:IDX, 3:REG
%macro SXMMN    3
             pinsrd  %1, %3, (%2 & 3)
             %endmacro

;
;args 1:XMM, 2:REG, 3:IDX
;
%macro LXMMN    3
             pextrd  %2, %1, (%3 & 3)
             %endmacro
%else
;
; Define SSE macros using SSE 2 instructions
; args 1:XMM, 2:IDX, 3:REG
%macro SXMMN    3
             pinsrw  %1, %3, (%2 & 3) * 2
             ror     %3, 16
             pinsrw  %1, %3, (%2 & 3) * 2 + 1
             rol     %3, 16
             %endmacro

;
;args 1:XMM, 2:REG, 3:IDX
;
%macro LXMMN    3
             pshufd  %1, %1, ((0E4E4E4h >> (%3 * 2))  & 0FFh)
             movd    %2, %1
             pshufd  %1, %1, ((0E4E4E4h >> (%3 * 2 + (%3 & 1) * 4)) & 0FFh)
             %endmacro
%endif

;
; XMM7 to save/restore EBP - slot 0, EBX - slot 1, ESI - slot 2, EDI - slot 3
;
%macro SAVE_REGS    0
  SXMMN      xmm7, 0, ebp
  SXMMN      xmm7, 1, ebx
  SXMMN      xmm7, 2, esi
  SXMMN      xmm7, 3, edi
  SAVE_ESP
             %endmacro

%macro LOAD_REGS    0
  LXMMN      xmm7, ebp, 0
  LXMMN      xmm7, ebx, 1
  LXMMN      xmm7, esi, 2
  LXMMN      xmm7, edi, 3
  LOAD_ESP
             %endmacro

;
; XMM6 to save/restore ESP - slot 0, EAX - slot 1, EDX - slot 2, ECX - slot 3
;
%macro LOAD_ESP    0
  movd       esp,  xmm6
             %endmacro

%macro SAVE_ESP    0
  SXMMN      xmm6, 0, esp
             %endmacro

%macro LOAD_EAX    0
  LXMMN      xmm6, eax, 1
             %endmacro

%macro SAVE_EAX    0
  SXMMN      xmm6, 1, eax
             %endmacro

%macro LOAD_EDX    0
  LXMMN      xmm6, edx, 2
             %endmacro

%macro SAVE_EDX    0
  SXMMN      xmm6, 2, edx
             %endmacro

%macro LOAD_ECX    0
  LXMMN      xmm6, ecx, 3
             %endmacro

%macro SAVE_ECX    0
  SXMMN      xmm6, 3, ecx
             %endmacro

;
; XMM5 slot 0 for calling stack
; arg 1:Entry
%macro CALL_XMM       1
             mov     esi, %%ReturnAddress
             SXMMN   xmm5, 0, esi
             mov     esi,  %1
             jmp     esi
%%ReturnAddress:
             %endmacro

%macro RET_XMM       0
             LXMMN   xmm5, esi, 0
             jmp     esi
             %endmacro

;
; XMM5 slot 1 for uCode status
;
%macro LOAD_UCODE_STATUS    0
  LXMMN      xmm5, eax, 1
             %endmacro

%macro SAVE_UCODE_STATUS    0
  SXMMN      xmm5, 1, eax
             %endmacro

;
; XMM5 slot 2 for TemporaryRamSize
;
%macro LOAD_TEMPORARY_RAM_SIZE    1
  LXMMN      xmm5, %1, 2
             %endmacro

%macro SAVE_TEMPORARY_RAM_SIZE    1
  SXMMN      xmm5, 2, %1
             %endmacro

%macro ENABLE_SSE   0
            ;
            ; Initialize floating point units
            ;
            jmp     NextAddress
align 4
            ;
            ; Float control word initial value:
            ; all exceptions masked, double-precision, round-to-nearest
            ;
FpuControlWord       DW      027Fh
            ;
            ; Multimedia-extensions control word:
            ; all exceptions masked, round-to-nearest, flush to zero for masked underflow
            ;
MmxControlWord       DD      01F80h
SseError:
            ;
            ; Processor has to support SSE
            ;
            jmp     SseError
NextAddress:
            finit
            fldcw   [FpuControlWord]

            ;
            ; Use CpuId instruction (CPUID.01H:EDX.SSE[bit 25] = 1) to test
            ; whether the processor supports SSE instruction.
            ;
            ; Save EBX to MM2
            ;
            movd    mm2, ebx
            mov     eax, 1
            cpuid
            bt      edx, 25
            jnc     SseError

%ifdef USE_SSE41_FLAG
            ;
            ; SSE 4.1 support
            ;
            bt      ecx, 19
            jnc     SseError
%endif
            ;
            ; Restore EBX from MM2
            ;
            movd    ebx, mm2

            ;
            ; Set OSFXSR bit (bit #9) & OSXMMEXCPT bit (bit #10)
            ;
            mov     eax, cr4
            or      eax, 00000600h
            mov     cr4, eax

            ;
            ; The processor should support SSE instruction and we can use
            ; ldmxcsr instruction
            ;
            ldmxcsr [MmxControlWord]
            %endmacro
