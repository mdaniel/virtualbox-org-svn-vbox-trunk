#
# Spec file for creating VirtualBox rpm packages
#

#
# Copyright (C) 2006-2015 Oracle Corporation
#
# This file is part of VirtualBox Open Source Edition (OSE), as
# available from http://www.virtualbox.org. This file is free software;
# you can redistribute it and/or modify it under the terms of the GNU
# General Public License (GPL) as published by the Free Software
# Foundation, in version 2 as it comes in the "COPYING" file of the
# VirtualBox OSE distribution. VirtualBox OSE is distributed in the
# hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
#

%define %SPEC% 1
%define %OSE% 1
%define %PYTHON% 1
%define VBOXDOCDIR %{_defaultdocdir}/%NAME%
%global __requires_exclude_from ^/usr/lib/virtualbox/VBoxPython.*$
%{!?python_sitelib: %define python_sitelib %(%{__python} -c "from distutils.sysconfig import get_python_lib; print get_python_lib()")}

Summary:   Oracle VM VirtualBox
Name:      %NAME%
Version:   %BUILDVER%_%BUILDREL%
Release:   1
URL:       http://www.virtualbox.org/
Source:    VirtualBox.tar
License:   GPLv2
Group:     Applications/System
Vendor:    Oracle Corporation
BuildRoot: %BUILDROOT%
Requires:  %INITSCRIPTS% %LIBASOUND% %NETTOOLS%

%if %{?rpm_suse:1}%{!?rpm_suse:0}
%debug_package
%endif

%MACROSPYTHON%

# our Qt5 libs are built on EL5 with ld 2.17 which does not provide --link-id=
%undefine _missing_build_ids_terminate_build


%description
VirtualBox is a powerful PC virtualization solution allowing
you to run a wide range of PC operating systems on your Linux
system. This includes Windows, Linux, FreeBSD, DOS, OpenBSD
and others. VirtualBox comes with a broad feature set and
excellent performance, making it the premier virtualization
software solution on the market.


%prep
%setup -q
DESTDIR=""
unset DESTDIR


%build


%install
# Mandriva: prevent replacing 'echo' by 'gprintf'
export DONT_GPRINTIFY=1
rm -rf $RPM_BUILD_ROOT
install -m 755 -d $RPM_BUILD_ROOT/sbin
install -m 755 -d $RPM_BUILD_ROOT%{_initrddir}
install -m 755 -d $RPM_BUILD_ROOT/lib/modules
install -m 755 -d $RPM_BUILD_ROOT/etc/vbox
install -m 755 -d $RPM_BUILD_ROOT/usr/bin
install -m 755 -d $RPM_BUILD_ROOT/usr/src
install -m 755 -d $RPM_BUILD_ROOT/usr/share/applications
install -m 755 -d $RPM_BUILD_ROOT/usr/share/pixmaps
install -m 755 -d $RPM_BUILD_ROOT/usr/share/icons/hicolor
install -m 755 -d $RPM_BUILD_ROOT%{VBOXDOCDIR}
install -m 755 -d $RPM_BUILD_ROOT/usr/lib/virtualbox
install -m 755 -d $RPM_BUILD_ROOT/usr/share/virtualbox
install -m 755 -d $RPM_BUILD_ROOT/usr/share/mime/packages
%if %{?with_python:1}%{!?with_python:0}
(export VBOX_INSTALL_PATH=/usr/lib/virtualbox && \
  cd ./sdk/installer && \
  %{__python} ./vboxapisetup.py install --prefix %{_prefix} --root $RPM_BUILD_ROOT)
%endif
rm -rf sdk/installer
mv nls $RPM_BUILD_ROOT/usr/share/virtualbox
cp -a src $RPM_BUILD_ROOT/usr/share/virtualbox
mv VBox.sh $RPM_BUILD_ROOT/usr/bin/VBox
mv VBoxSysInfo.sh $RPM_BUILD_ROOT/usr/share/virtualbox
cp icons/128x128/virtualbox.png $RPM_BUILD_ROOT/usr/share/pixmaps/virtualbox.png
cd icons
  for i in *; do
    if [ -f $i/virtualbox.* ]; then
      install -d $RPM_BUILD_ROOT/usr/share/icons/hicolor/$i/apps
      mv $i/virtualbox.* $RPM_BUILD_ROOT/usr/share/icons/hicolor/$i/apps
    fi
    install -d $RPM_BUILD_ROOT/usr/share/icons/hicolor/$i/mimetypes
    mv $i/* $RPM_BUILD_ROOT/usr/share/icons/hicolor/$i/mimetypes || true
    rmdir $i
  done
cd -
rmdir icons
mv virtualbox.xml $RPM_BUILD_ROOT/usr/share/mime/packages
mv VBoxTunctl $RPM_BUILD_ROOT/usr/bin
%if %{?is_ose:0}%{!?is_ose:1}
for d in /lib/modules/*; do
  if [ -L $d/build ]; then
    rm -f /tmp/vboxdrv-Module.symvers
    ./src/vboxhost/build_in_tmp \
      --save-module-symvers /tmp/vboxdrv-Module.symvers \
      --module-source `pwd`/src/vboxhost/vboxdrv \
      KBUILD_VERBOSE= KERN_DIR=$d/build MODULE_DIR=$RPM_BUILD_ROOT/$d/misc -j4 \
      %INSTMOD%
    ./src/vboxhost/build_in_tmp \
      --use-module-symvers /tmp/vboxdrv-Module.symvers \
      --module-source `pwd`/src/vboxhost/vboxnetflt \
      KBUILD_VERBOSE= KERN_DIR=$d/build MODULE_DIR=$RPM_BUILD_ROOT/$d/misc -j4 \
      %INSTMOD%
    ./src/vboxhost/build_in_tmp \
      --use-module-symvers /tmp/vboxdrv-Module.symvers \
      --module-source `pwd`/src/vboxhost/vboxnetadp \
      KBUILD_VERBOSE= KERN_DIR=$d/build MODULE_DIR=$RPM_BUILD_ROOT/$d/misc -j4 \
      %INSTMOD%
    ./src/vboxhost/build_in_tmp \
      --use-module-symvers /tmp/vboxdrv-Module.symvers \
      --module-source `pwd`/src/vboxhost/vboxpci \
      KBUILD_VERBOSE= KERN_DIR=$d/build MODULE_DIR=$RPM_BUILD_ROOT/$d/misc -j4 \
      %INSTMOD%
  fi
done
rm -r src
%endif
%if %{?is_ose:0}%{!?is_ose:1}
  for i in rdesktop-vrdp.tar.gz rdesktop-vrdp-keymaps; do
    mv $i $RPM_BUILD_ROOT/usr/share/virtualbox; done
  mv rdesktop-vrdp $RPM_BUILD_ROOT/usr/bin
%endif
for i in additions/VBoxGuestAdditions.iso; do
  mv $i $RPM_BUILD_ROOT/usr/share/virtualbox; done
ln -s VBox $RPM_BUILD_ROOT/usr/bin/VirtualBox
ln -s VBox $RPM_BUILD_ROOT/usr/bin/virtualbox
ln -s VBox $RPM_BUILD_ROOT/usr/bin/VBoxManage
ln -s VBox $RPM_BUILD_ROOT/usr/bin/vboxmanage
test -f VBoxSDL && ln -s VBox $RPM_BUILD_ROOT/usr/bin/VBoxSDL
test -f VBoxSDL && ln -s VBox $RPM_BUILD_ROOT/usr/bin/vboxsdl
ln -s VBox $RPM_BUILD_ROOT/usr/bin/VBoxVRDP
ln -s VBox $RPM_BUILD_ROOT/usr/bin/VBoxHeadless
ln -s VBox $RPM_BUILD_ROOT/usr/bin/vboxheadless
ln -s VBox $RPM_BUILD_ROOT/usr/bin/VBoxDTrace
ln -s VBox $RPM_BUILD_ROOT/usr/bin/vboxdtrace
ln -s VBox $RPM_BUILD_ROOT/usr/bin/VBoxBugReport
ln -s VBox $RPM_BUILD_ROOT/usr/bin/vboxbugreport
ln -s VBox $RPM_BUILD_ROOT/usr/bin/VBoxBalloonCtrl
ln -s VBox $RPM_BUILD_ROOT/usr/bin/vboxballoonctrl
ln -s VBox $RPM_BUILD_ROOT/usr/bin/VBoxAutostart
ln -s VBox $RPM_BUILD_ROOT/usr/bin/vboxautostart
ln -s VBox $RPM_BUILD_ROOT/usr/bin/vboxwebsrv
ln -s /usr/lib/virtualbox/vbox-img $RPM_BUILD_ROOT/usr/bin/vbox-img
ln -s /usr/share/virtualbox/src/vboxhost $RPM_BUILD_ROOT/usr/src/vboxhost-%VER%
mv virtualbox.desktop $RPM_BUILD_ROOT/usr/share/applications/virtualbox.desktop
mv VBox.png $RPM_BUILD_ROOT/usr/share/pixmaps/VBox.png
%{!?is_ose: mv LICENSE $RPM_BUILD_ROOT%{VBOXDOCDIR}}
mv UserManual*.pdf $RPM_BUILD_ROOT%{VBOXDOCDIR}
%{!?is_ose: mv VirtualBox*.chm $RPM_BUILD_ROOT%{VBOXDOCDIR}}
install -m 755 -d $RPM_BUILD_ROOT/usr/lib/debug/usr/lib/virtualbox
%if %{?rpm_suse:1}%{!?rpm_suse:0}
rm *.debug
%else
mv *.debug $RPM_BUILD_ROOT/usr/lib/debug/usr/lib/virtualbox
%endif
mv * $RPM_BUILD_ROOT/usr/lib/virtualbox
if [ -f $RPM_BUILD_ROOT/usr/lib/virtualbox/libQt5CoreVBox.so.5 ]; then
  $RPM_BUILD_ROOT/usr/lib/virtualbox/chrpath --keepgoing --replace /usr/lib/virtualbox \
    $RPM_BUILD_ROOT/usr/lib/virtualbox/*.so.5 \
    $RPM_BUILD_ROOT/usr/lib/virtualbox/plugins/platforms/*.so \
    $RPM_BUILD_ROOT/usr/lib/virtualbox/plugins/xcbglintegrations/*.so || true
  echo "[Paths]" > $RPM_BUILD_ROOT/usr/lib/virtualbox/qt.conf
  echo "Plugins = /usr/lib/virtualbox/plugins" >> $RPM_BUILD_ROOT/usr/lib/virtualbox/qt.conf
  rm $RPM_BUILD_ROOT/usr/lib/virtualbox/chrpath
fi
if [ -d $RPM_BUILD_ROOT/usr/lib/virtualbox/legacy ]; then
  mv $RPM_BUILD_ROOT/usr/lib/virtualbox/legacy/* $RPM_BUILD_ROOT/usr/lib/virtualbox
  rmdir $RPM_BUILD_ROOT/usr/lib/virtualbox/legacy
fi
ln -s ../VBoxVMM.so $RPM_BUILD_ROOT/usr/lib/virtualbox/components/VBoxVMM.so
for i in VirtualBox VBoxHeadless VBoxNetDHCP VBoxNetNAT VBoxNetAdpCtl; do
  chmod 4511 $RPM_BUILD_ROOT/usr/lib/virtualbox/$i; done
if [ -f $RPM_BUILD_ROOT/usr/lib/virtualbox/VBoxVolInfo ]; then
  chmod 4511 $RPM_BUILD_ROOT/usr/lib/virtualbox/VBoxVolInfo
fi
test -f $RPM_BUILD_ROOT/usr/lib/virtualbox/VBoxSDL && \
  chmod 4511 $RPM_BUILD_ROOT/usr/lib/virtualbox/VBoxSDL


%pre
# defaults
[ -r /etc/default/virtualbox ] && . /etc/default/virtualbox

# check for old installation
if [ -r /etc/vbox/vbox.cfg ]; then
  . /etc/vbox/vbox.cfg
  if [ "x$INSTALL_DIR" != "x" -a -d "$INSTALL_DIR" ]; then
    echo "An old installation of VirtualBox was found. To install this package the"
    echo "old package has to be removed first. Have a look at /etc/vbox/vbox.cfg to"
    echo "determine the installation directory of the previous installation. After"
    echo "uninstalling the old package remove the file /etc/vbox/vbox.cfg."
    exit 1
  fi
fi

# check for active VMs of the installed (old) package
# Execute the installed packages pre-uninstaller if present.
/usr/lib/virtualbox/prerm-common.sh 2>/dev/null
# Stop services from older versions without pre-uninstaller.
/etc/init.d/vboxballoonctrl-service stop 2>/dev/null
/etc/init.d/vboxautostart-service stop 2>/dev/null
/etc/init.d/vboxweb-service stop 2>/dev/null
VBOXSVC_PID=`pidof VBoxSVC 2>/dev/null || true`
if [ -n "$VBOXSVC_PID" ]; then
  # ask the daemon to terminate immediately
  kill -USR1 $VBOXSVC_PID
  sleep 1
  if pidof VBoxSVC > /dev/null 2>&1; then
    echo "A copy of VirtualBox is currently running.  Please close it and try again."
    echo "Please note that it can take up to ten seconds for VirtualBox (in particular"
    echo "the VBoxSVC daemon) to finish running."
    exit 1
  fi
fi


%post
LOG="/var/log/vbox-install.log"

# defaults
[ -r /etc/default/virtualbox ] && . /etc/default/virtualbox

# remove old cruft
if [ -f /etc/init.d/vboxdrv.sh ]; then
  echo "Found old version of /etc/init.d/vboxdrv.sh, removing."
  rm /etc/init.d/vboxdrv.sh
fi
if [ -f /etc/vbox/vbox.cfg ]; then
  echo "Found old version of /etc/vbox/vbox.cfg, removing."
  rm /etc/vbox/vbox.cfg
fi
rm -f /etc/vbox/module_not_compiled

# create users groups (disable with INSTALL_NO_GROUP=1 in /etc/default/virtualbox)
if [ "$INSTALL_NO_GROUP" != "1" ]; then
  echo
  echo "Creating group 'vboxusers'. VM users must be member of that group!"
  echo
  groupadd -r -f vboxusers 2> /dev/null
fi

%if %{?rpm_mdv:1}%{!?rpm_mdv:0}
/sbin/ldconfig
%update_menus
%endif
update-mime-database /usr/share/mime &> /dev/null || :
update-desktop-database -q > /dev/null 2>&1 || :
touch --no-create /usr/share/icons/hicolor
gtk-update-icon-cache -q /usr/share/icons/hicolor 2> /dev/null || :

# Disable module compilation with INSTALL_NO_VBOXDRV=1 in /etc/default/virtualbox
if test "${INSTALL_NO_VBOXDRV}" = 1; then
  POSTINST_START=--nostart
else
  POSTINST_START=
fi
# Install and start the new service scripts.
/usr/lib/virtualbox/prerm-common.sh || true
/usr/lib/virtualbox/postinst-common.sh ${POSTINST_START} > /dev/null || true


%preun
# Called before the package is removed, or during upgrade after (not before)
# the new version's "post" scriptlet. 
# $1==0: remove the last version of the package
# $1>=1: upgrade
if [ "$1" = 0 ]; then
  /usr/lib/virtualbox/prerm-common.sh || exit 1
  rm -f /etc/udev/rules.d/60-vboxdrv.rules
  rm -f /etc/vbox/license_agreed
  rm -f /etc/vbox/module_not_compiled
fi

%postun
%if %{?rpm_mdv:1}%{!?rpm_mdv:0}
/sbin/ldconfig
%{clean_desktop_database}
%clean_menus
%endif
update-mime-database /usr/share/mime &> /dev/null || :
update-desktop-database -q > /dev/null 2>&1 || :
touch --no-create /usr/share/icons/hicolor
gtk-update-icon-cache -q /usr/share/icons/hicolor 2> /dev/null || :
rm -rf /usr/lib/virtualbox/ExtensionPacks


%clean
rm -rf $RPM_BUILD_ROOT


%files
%defattr(-,root,root)
%doc %{VBOXDOCDIR}/*
%if %{?with_python:1}%{!?with_python:0}
%{?rpm_suse: %{py_sitedir}/*}
%{!?rpm_suse: %{python_sitelib}/*}
%endif
/etc/vbox
/usr/bin/*
/usr/src/vbox*
/usr/lib/virtualbox
/usr/share/applications/*
/usr/share/icons/hicolor/*/apps/*
/usr/share/icons/hicolor/*/mimetypes/*
/usr/share/mime/packages/*
/usr/share/pixmaps/*
/usr/share/virtualbox
