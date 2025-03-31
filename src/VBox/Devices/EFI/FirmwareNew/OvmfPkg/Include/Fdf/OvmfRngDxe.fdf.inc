##
#    SPDX-License-Identifier: BSD-2-Clause-Patent
##

INF  SecurityPkg/RandomNumberGenerator/RngDxe/RngDxe.inf
!ifndef $(VBOX)
INF  OvmfPkg/VirtioRngDxe/VirtioRng.inf
!endif
