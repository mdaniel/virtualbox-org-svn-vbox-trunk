; $Id$
;; @file
; Disassembly testcase - Valid pop sequences and related instructions.
;
; This is a build test, that means it will be assembled, disassembled,
; then the disassembly output will be assembled and the new binary will
; compared with the original.
;

;
; Copyright (C) 2008-2019 Oracle Corporation
;
; This file is part of VirtualBox Open Source Edition (OSE), as
; available from http://www.virtualbox.org. This file is free software;
; you can redistribute it and/or modify it under the terms of the GNU
; General Public License (GPL) as published by the Free Software
; Foundation, in version 2 as it comes in the "COPYING" file of the
; VirtualBox OSE distribution. VirtualBox OSE is distributed in the
; hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
;

    BITS TEST_BITS
%if TEST_BITS != 64
    pop     bp
    pop     ebp
    pop     word [bp]
    pop     dword [bp]
    pop     word [ebp]
    pop     dword [ebp]
%else
 %if 0 ; doesn't work yet
    pop     rbp
    pop     qword [rbp]
 %endif
%endif

