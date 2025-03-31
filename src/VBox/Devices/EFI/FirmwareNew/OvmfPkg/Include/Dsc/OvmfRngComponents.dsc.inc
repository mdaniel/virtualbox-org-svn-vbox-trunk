##
#    SPDX-License-Identifier: BSD-2-Clause-Patent
##

  SecurityPkg/RandomNumberGenerator/RngDxe/RngDxe.inf {
    <LibraryClasses>
      RngLib|MdePkg/Library/BaseRngLib/BaseRngLib.inf
  }
!ifndef $(VBOX)
  OvmfPkg/VirtioRngDxe/VirtioRng.inf
!endif
