;------------------------------------------------------------------------------ ;
; Copyright (c) 2006 - 2015, Intel Corporation. All rights reserved.<BR>
; This program and the accompanying materials
; are licensed and made available under the terms and conditions of the BSD License
; which accompanies this distribution.  The full text of the license may be found at
; http://opensource.org/licenses/bsd-license.php.
;
; THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
; WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
;
; Module Name:
;
;   MpFuncs.asm
;
; Abstract:
;
;   This is the assembly code for Multi-processor S3 support
;
;-------------------------------------------------------------------------------

EXTERN  InitializeFloatingPointUnits:PROC

VacantFlag             Equ   00h
NotVacantFlag          Equ   0ffh

LockLocation                  equ        RendezvousFunnelProcEnd - RendezvousFunnelProcStart
StackStartAddressLocation     equ        LockLocation + 08h
StackSizeLocation             equ        LockLocation + 10h
CProcedureLocation            equ        LockLocation + 18h
GdtrLocation                  equ        LockLocation + 20h
IdtrLocation                  equ        LockLocation + 2Ah
BufferStartLocation           equ        LockLocation + 34h
Cr3OffsetLocation             equ        LockLocation + 38h

;-------------------------------------------------------------------------------------
;RendezvousFunnelProc  procedure follows. All APs execute their procedure. This
;procedure serializes all the AP processors through an Init sequence. It must be
;noted that APs arrive here very raw...ie: real mode, no stack.
;ALSO THIS PROCEDURE IS EXECUTED BY APs ONLY ON 16 BIT MODE. HENCE THIS PROC
;IS IN MACHINE CODE.
;-------------------------------------------------------------------------------------
;RendezvousFunnelProc (&WakeUpBuffer,MemAddress);

;text      SEGMENT
.code

RendezvousFunnelProc   PROC
RendezvousFunnelProcStart::

; At this point CS = 0x(vv00) and ip= 0x0.

        db 8ch,  0c8h                 ; mov        ax,  cs
        db 8eh,  0d8h                 ; mov        ds,  ax
        db 8eh,  0c0h                 ; mov        es,  ax
        db 8eh,  0d0h                 ; mov        ss,  ax
        db 33h,  0c0h                 ; xor        ax,  ax
        db 8eh,  0e0h                 ; mov        fs,  ax
        db 8eh,  0e8h                 ; mov        gs,  ax

flat32Start::

        db 0BEh
        dw BufferStartLocation        ; mov        si, BufferStartLocation
        db 66h,  8Bh, 14h             ; mov        edx,dword ptr [si]          ; EDX is keeping the start address of wakeup buffer

        db 0BEh
        dw Cr3OffsetLocation          ; mov        si, Cr3Location
        db 66h,  8Bh, 0Ch             ; mov        ecx,dword ptr [si]          ; ECX is keeping the value of CR3

        db 0BEh
        dw GdtrLocation               ; mov        si, GdtrProfile
        db 66h                        ; db         66h
        db 2Eh,  0Fh, 01h, 14h        ; lgdt       fword ptr cs:[si]

        db 0BEh
        dw IdtrLocation               ; mov        si, IdtrProfile
        db 66h                        ; db         66h
        db 2Eh,  0Fh, 01h, 1Ch        ; lidt       fword ptr cs:[si]

        db 33h,  0C0h                 ; xor        ax,  ax
        db 8Eh,  0D8h                 ; mov        ds,  ax

        db 0Fh,  20h, 0C0h            ; mov        eax, cr0                    ; Get control register 0
        db 66h,  83h, 0C8h, 01h       ; or         eax, 000000001h             ; Set PE bit (bit #0)
        db 0Fh,  22h, 0C0h            ; mov        cr0, eax

FLAT32_JUMP::

        db 66h,  67h, 0EAh            ; far jump
        dd 0h                         ; 32-bit offset
        dw 20h                        ; 16-bit selector

PMODE_ENTRY::                         ; protected mode entry point

        db 66h,  0B8h, 18h,  00h      ; mov        ax,  18h
        db 66h,  8Eh,  0D8h           ; mov        ds,  ax
        db 66h,  8Eh,  0C0h           ; mov        es,  ax
        db 66h,  8Eh,  0E0h           ; mov        fs,  ax
        db 66h,  8Eh,  0E8h           ; mov        gs,  ax
        db 66h,  8Eh,  0D0h           ; mov        ss,  ax                     ; Flat mode setup.

        db 0Fh,  20h,  0E0h           ; mov        eax, cr4
        db 0Fh,  0BAh, 0E8h, 05h      ; bts        eax, 5
        db 0Fh,  22h,  0E0h           ; mov        cr4, eax

        db 0Fh,  22h,  0D9h           ; mov        cr3, ecx

        db 8Bh,  0F2h                 ; mov        esi, edx                    ; Save wakeup buffer address

        db 0B9h
        dd 0C0000080h                 ; mov        ecx, 0c0000080h             ; EFER MSR number.
        db 0Fh,  32h                  ; rdmsr                                  ; Read EFER.
        db 0Fh,  0BAh, 0E8h, 08h      ; bts        eax, 8                      ; Set LME=1.
        db 0Fh,  30h                  ; wrmsr                                  ; Write EFER.

        db 0Fh,  20h,  0C0h           ; mov        eax, cr0                    ; Read CR0.
        db 0Fh,  0BAh, 0E8h, 1Fh      ; bts        eax, 31                     ; Set PG=1.
        db 0Fh,  22h,  0C0h           ; mov        cr0, eax                    ; Write CR0.

LONG_JUMP::

        db 67h,  0EAh                 ; far jump
        dd 0h                         ; 32-bit offset
        dw 38h                        ; 16-bit selector

LongModeStart::

        mov         ax,  30h
        mov         ds,  ax
        mov         es,  ax
        mov         ss,  ax

        mov  edi, esi
        add  edi, LockLocation
        mov  al,  NotVacantFlag
TestLock::
        xchg byte ptr [edi], al
        cmp  al, NotVacantFlag
        jz   TestLock

ProgramStack::

        mov  edi, esi
        add  edi, StackSizeLocation
        mov  rax, qword ptr [edi]
        mov  edi, esi
        add  edi, StackStartAddressLocation
        add  rax, qword ptr [edi]
        mov  rsp, rax
        mov  qword ptr [edi], rax

Releaselock::

        mov  al,  VacantFlag
        mov  edi, esi
        add  edi, LockLocation
        xchg byte ptr [edi], al

        ;
        ; Call assembly function to initialize FPU.
        ;
        mov         rax, InitializeFloatingPointUnits
        sub         rsp, 20h
        call        rax
        add         rsp, 20h

        ;
        ; Call C Function
        ;
        mov         edi, esi
        add         edi, CProcedureLocation
        mov         rax, qword ptr [edi]

        test        rax, rax
        jz          GoToSleep

        sub         rsp, 20h
        call        rax
        add         rsp, 20h

GoToSleep::
        cli
        hlt
        jmp         $-2

RendezvousFunnelProcEnd::
RendezvousFunnelProc   ENDP


;-------------------------------------------------------------------------------------
;  AsmGetAddressMap (&AddressMap);
;-------------------------------------------------------------------------------------
; comments here for definition of address map
AsmGetAddressMap   PROC
        mov         rax, offset RendezvousFunnelProcStart
        mov         qword ptr [rcx], rax
        mov         qword ptr [rcx+8h], PMODE_ENTRY - RendezvousFunnelProcStart
        mov         qword ptr [rcx+10h], FLAT32_JUMP - RendezvousFunnelProcStart
        mov         qword ptr [rcx+18h], RendezvousFunnelProcEnd - RendezvousFunnelProcStart
        mov         qword ptr [rcx+20h], LongModeStart - RendezvousFunnelProcStart
        mov         qword ptr [rcx+28h], LONG_JUMP - RendezvousFunnelProcStart
        ret

AsmGetAddressMap   ENDP

END
