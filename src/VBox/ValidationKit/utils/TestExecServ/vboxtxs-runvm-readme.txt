$Id$


VirtualBox Test eXecution Service
=================================

This readme briefly describes how to install the Test eXecution Service (TXS)
for nested hardware-virtualization smoke testing on the various systems.

The basic idea is to execute one smoke test within the VM and then launch
the regular TXS service in the VM to report success or failure to the host.

Linux Installation
------------------

1.  scp/download latest release build of VirtualBox and install it in the VM.
2.  scp/download the required smoke test VDI from remote test-resource to
    /home/vbox/testrsrc/* (preserve the directory layout)
3.  cd /root
3.  scp/download VBoxValidationKit*.zip there.
5.  unzip VBoxValidationKit*.zip
6.  chmod -R u+w,a+x /opt/validationkit/
7a. initrc: TODO: document init.rc setup
7b. systemd: Link/copy the vboxtxs-runvm.system to [/usr]/lib/systemd/system/
    Enable the vboxtxs-runvm service, usually using:
       systemctl enable vboxtxs-runvm
8.  Check the cdrom location in /opt/validationkit/linux/vboxtxs-runvm and fix
    it so it's correct.
9.  reboot / done.

TODO: Document other OSes as we add them.

Note: vboxtxs-runvm uses a GUI session to launch the nested-VM for better
visibility when troubleshooting the nested smoke test.

If this causes problems try troubleshooting the XAUTHORITY and DISPLAY
environment variables in vboxtxs-runvm.service. It might differ depending
on the display manager of the particular linux distro.



Testing the setup
-----------------

1. Make sure the validationkit.iso is inserted.
2. Boot / reboot the guest.
3. To test the connection - Depending on the TXS transport options:
      nat)   python testdriver/tst-txsclient.py --reversed-setup
      other) python testdriver/tst-txsclient.py --hostname <guest-ip>
4. To test the smoke test:
      python tests/smoketests/tdSmokeTest1.py -v -v -d --vbox-session-type gui --test-vms <guest-name>

