; $Id$
;; @file
; Disassembly testcase - Valid mov from/to segment instructions.
;
; This is a build test, that means it will be assembled, disassembled,
; then the disassembly output will be assembled and the new binary will
; compared with the original.
;

;
; Copyright (C) 2008-2024 Oracle and/or its affiliates.
;
; This file is part of VirtualBox base platform packages, as
; available from https://www.virtualbox.org.
;
; This program is free software; you can redistribute it and/or
; modify it under the terms of the GNU General Public License
; as published by the Free Software Foundation, in version 3 of the
; License.
;
; This program is distributed in the hope that it will be useful, but
; WITHOUT ANY WARRANTY; without even the implied warranty of
; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
; General Public License for more details.
;
; You should have received a copy of the GNU General Public License
; along with this program; if not, see <https://www.gnu.org/licenses>.
;
; SPDX-License-Identifier: GPL-3.0-only
;

    BITS TEST_BITS

    mov fs, eax
    mov fs, ax
%if TEST_BITS == 64
    mov fs, rax
%endif

    mov fs, [ebx]
%if TEST_BITS != 64
    mov fs, [bx]
%else
    mov fs, [rbx]
%endif

    mov ax, fs
    mov eax, fs
%if TEST_BITS == 64
    mov rax, fs
%endif

    mov [ebx], fs
%if TEST_BITS != 64
    mov [bx], fs
%else
    mov [rbx], fs
%endif
