# Oracle VM VirtualBox
# VirtualBox installer shell routines
#

#
# Copyright (C) 2007-2017 Oracle Corporation
#
# This file is part of VirtualBox Open Source Edition (OSE), as
# available from http://www.virtualbox.org. This file is free software;
# you can redistribute it and/or modify it under the terms of the GNU
# General Public License (GPL) as published by the Free Software
# Foundation, in version 2 as it comes in the "COPYING" file of the
# VirtualBox OSE distribution. VirtualBox OSE is distributed in the
# hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
#

ro_LOG_FILE=""
ro_X11_AUTOSTART="/etc/xdg/autostart"
ro_KDE_AUTOSTART="/usr/share/autostart"

## Aborts the script and prints an error message to stderr.
#
# syntax: abort message

abort()
{
    echo 1>&2 "$1"
    exit 1
}

## Creates an empty log file and remembers the name for future logging
# operations
create_log()
{
    ## The path of the file to create.
    ro_LOG_FILE="$1"
    if [ "$ro_LOG_FILE" = "" ]; then
        abort "create_log called without an argument!  Aborting..."
    fi
    # Create an empty file
    echo > "$ro_LOG_FILE" 2> /dev/null
    if [ ! -f "$ro_LOG_FILE" -o "`cat "$ro_LOG_FILE"`" != "" ]; then
        abort "Error creating log file!  Aborting..."
    fi
}

## Writes text to standard error
#
# Syntax: info text
info()
{
    echo 1>&2 "$1"
}

## Writes text to the log file
#
# Syntax: log text
log()
{
    if [ "$ro_LOG_FILE" = "" ]; then
        abort "Error!  Logging has not been set up yet!  Aborting..."
    fi
    echo "$1" >> $ro_LOG_FILE
    return 0
}

## Writes test to standard output and to the log file
#
# Syntax: infolog text
infolog()
{
    info "$1"
    log "$1"
}

## Checks whether a module is loaded with a given string in its name.
#
# syntax: module_loaded string
module_loaded()
{
    if [ "$1" = "" ]; then
        log "module_loaded called without an argument.  Aborting..."
        abort "Error in installer.  Aborting..."
    fi
    lsmod | grep -q $1
}

## Abort if we are not running as root
check_root()
{
    if [ `id -u` -ne 0 ]; then
        abort "This program must be run with administrator privileges.  Aborting"
    fi
}

## Abort if dependencies are not found
check_deps()
{
    for i in ${@}; do
        type "${i}" >/dev/null 2>&1 ||
            abort "${i} not found.  Please install: ${*}; and try again."
    done
}

## Abort if a copy of VirtualBox is already running
check_running()
{
    VBOXSVC_PID=`pidof VBoxSVC 2> /dev/null`
    if [ -n "$VBOXSVC_PID" ]; then
        if [ -f /etc/init.d/vboxweb-service ]; then
            kill -USR1 $VBOXSVC_PID
        fi
        sleep 1
        if pidof VBoxSVC > /dev/null 2>&1; then
            echo 1>&2 "A copy of VirtualBox is currently running.  Please close it and try again."
            abort "Please note that it can take up to ten seconds for VirtualBox to finish running."
        fi
    fi
}

## Creates a systemd wrapper in /lib for an LSB init script
systemd_wrap_init_script()
{
    self="systemd_wrap_init_script"
    ## The init script to be installed.  The file may be copied or referenced.
    script="$(readlink -f -- "${1}")"
    ## Name for the service.
    name="$2"
    test -x "$script" && test ! "$name" = "" || \
        { echo "$self: invalid arguments" >&2 && return 1; }
    test -d /usr/lib/systemd/system && unit_path=/usr/lib/systemd/system
    test -d /lib/systemd/system && unit_path=/lib/systemd/system
    test -n "${unit_path}" || \
        { echo "$self: systemd unit path not found" >&2 && return 1; }
    conflicts=`sed -n 's/# *X-Conflicts-With: *\(.*\)/\1/p' "${script}" | sed 's/\$[a-z]*//'`
    description=`sed -n 's/# *Short-Description: *\(.*\)/\1/p' "${script}"`
    required=`sed -n 's/# *Required-Start: *\(.*\)/\1/p' "${script}" | sed 's/\$[a-z]*//'`
    startbefore=`sed -n 's/# *X-Start-Before: *\(.*\)/\1/p' "${script}" | sed 's/\$[a-z]*//'`
    runlevels=`sed -n 's/# *Default-Start: *\(.*\)/\1/p' "${script}"`
    servicetype=`sed -n 's/# *X-Service-Type: *\(.*\)/\1/p' "${script}"`
    test -z "${servicetype}" && servicetype="forking"
    targets=`for i in ${runlevels}; do printf "runlevel${i}.target "; done`
    before=`for i in ${startbefore}; do printf "${i}.service "; done`
    after=`for i in ${required}; do printf "${i}.service "; done`
    cat > "${unit_path}/${name}.service" << EOF
[Unit]
SourcePath=${script}
Description=${description}
Before=${targets}shutdown.target ${before}
After=${after}
Conflicts=shutdown.target ${conflicts}

[Service]
Type=${servicetype}
Restart=no
TimeoutSec=5min
IgnoreSIGPIPE=no
KillMode=process
GuessMainPID=no
RemainAfterExit=yes
ExecStart=${script} start
ExecStop=${script} stop

[Install]
WantedBy=multi-user.target
EOF
    systemctl daemon-reexec
}

## Installs a file containing a shell script as an init script
install_init_script()
{
    self="install_init_script"
    ## The init script to be installed.  The file may be copied or referenced.
    script="$1"
    ## Name for the service.
    name="$2"

    test -x "${script}" && test ! "${name}" = "" ||
        { echo "${self}: invalid arguments" >&2; return 1; }
    # Do not unconditionally silence the following "ln".
    test -L "/sbin/rc${name}" && rm "/sbin/rc${name}"
    ln -s "${script}" "/sbin/rc${name}"
    if test -x "`which systemctl 2>/dev/null`"; then
        if ! test -f /sbin/init || ls -l /sbin/init | grep -q ">.*systemd"; then
            { systemd_wrap_init_script "$script" "$name"; return; }
        fi
    fi
    if test -d /etc/rc.d/init.d; then
        cp "${script}" "/etc/rc.d/init.d/${name}" &&
            chmod 755 "/etc/rc.d/init.d/${name}"
    elif test -d /etc/init.d; then
        cp "${script}" "/etc/init.d/${name}" &&
            chmod 755 "/etc/init.d/${name}"
    else
        { echo "${self}: error: unknown init type" >&2; return 1; }
    fi
}

## Remove the init script "name"
remove_init_script()
{
    self="remove_init_script"
    ## Name of the service to remove.
    name="$1"

    test -n "${name}" ||
        { echo "$self: missing argument"; return 1; }
    rm -f "/sbin/rc${name}"
    rm -f /lib/systemd/system/"$name".service /usr/lib/systemd/system/"$name".service
    rm -f "/etc/rc.d/init.d/$name"
    rm -f "/etc/init.d/$name"
}

## Did we install a systemd service?
systemd_service_installed()
{
    ## Name of service to test.
    name="${1}"

    test -f /lib/systemd/system/"${name}".service ||
        test -f /usr/lib/systemd/system/"${name}".service
}

## Perform an action on a service
do_sysvinit_action()
{
    self="do_sysvinit_action"
    ## Name of service to start.
    name="${1}"
    ## The action to perform, normally "start", "stop" or "status".
    action="${2}"

    test ! -z "${name}" && test ! -z "${action}" ||
        { echo "${self}: missing argument" >&2; return 1; }
    if systemd_service_installed "${name}"; then
        systemctl -q ${action} "${name}"
    elif test -x "/etc/rc.d/init.d/${name}"; then
        "/etc/rc.d/init.d/${name}" "${action}" quiet
    elif test -x "/etc/init.d/${name}"; then
        "/etc/init.d/${name}" "${action}" quiet
    fi
}

## Start a service
start_init_script()
{
    do_sysvinit_action "${1}" start
}

## Stop the init script "name"
stop_init_script()
{
    do_sysvinit_action "${1}" stop
}

## Extract chkconfig information from a sysvinit script.
get_chkconfig_info()
{
    ## The script to extract the information from.
    script="${1}"

    set `sed -n 's/# *chkconfig: *\([0-9]*\) *\(.*\)/\1 \2/p' "${script}"`
    ## Which runlevels should we start in?
    runlevels="${1}"
    ## How soon in the boot process will we start, from 00 (first) to 99
    start_order="${2}"
    ## How soon in the shutdown process will we stop, from 99 (first) to 00
    stop_order="${3}"
    test ! -z "${name}" || \
        { echo "${self}: missing name" >&2; return 1; }
    expr "${start_order}" + 0 > /dev/null 2>&1 && \
        expr 0 \<= "${start_order}" > /dev/null 2>&1 && \
        test `expr length "${start_order}"` -eq 2 > /dev/null 2>&1 || \
        { echo "${self}: start sequence number must be between 00 and 99" >&2;
            return 1; }
    expr "${stop_order}" + 0 > /dev/null 2>&1 && \
        expr 0 \<= "${stop_order}" > /dev/null 2>&1 && \
        test `expr length "${stop_order}"` -eq 2 > /dev/null 2>&1 || \
        { echo "${self}: stop sequence number must be between 00 and 99" >&2;
            return 1; }
}

## Add a service to its default runlevels (annotated inside the script, see get_chkconfig_info).
addrunlevel()
{
    self="addrunlevel"
    ## Service name.
    name="${1}"

    test -n "${name}" || \
        { echo "${self}: missing argument" >&2; return 1; }
    systemd_service_installed "${name}" && \
        { systemctl -q enable "${name}"; return; }
    if test -x "/etc/rc.d/init.d/${name}"; then
        init_d_path=/etc/rc.d
    elif test -x "/etc/init.d/${name}"; then
        init_d_path=/etc
    else
        { echo "${self}: error: unknown init type" >&2; return 1; }
    fi
    get_chkconfig_info "${init_d_path}/init.d/${name}" || return 1
    # Redhat based sysvinit systems
    if test -x "`which chkconfig 2>/dev/null`"; then
        chkconfig --add "${name}"
    # SUSE-based sysvinit systems
    elif test -x "`which insserv 2>/dev/null`"; then
        insserv "${name}"
    # Debian/Ubuntu-based systems
    elif test -x "`which update-rc.d 2>/dev/null`"; then
        # Old Debians did not support dependencies
        update-rc.d "${name}" defaults "${start_order}" "${stop_order}"
    # Gentoo Linux
    elif test -x "`which rc-update 2>/dev/null`"; then
        rc-update add "${name}" default
    # Generic sysvinit
    elif test -n "${init_d_path}/rc0.d"
    then
        for locali in 0 1 2 3 4 5 6
        do
            target="${init_d_path}/rc${locali}.d/K${stop_order}${name}"
            expr "${runlevels}" : ".*${locali}" >/dev/null && \
                target="${init_d_path}/rc${locali}.d/S${start_order}${name}"
            test -e "${init_d_path}/rc${locali}.d/"[KS][0-9]*"${name}" || \
                ln -fs "${init_d_path}/init.d/${name}" "${target}"
        done
    else
        { echo "${self}: error: unknown init type" >&2; return 1; }
    fi
}


## Delete a service from a runlevel
delrunlevel()
{
    self="delrunlevel"
    ## Service name.
    name="${1}"

    test -n "${name}" ||
        { echo "${self}: missing argument" >&2; return 1; }
    systemctl -q disable "${name}" >/dev/null 2>&1
    # Redhat-based systems
    chkconfig --del "${name}" >/dev/null 2>&1
    # SUSE-based sysvinit systems
    insserv -r "${name}" >/dev/null 2>&1
    # Debian/Ubuntu-based systems
    update-rc.d -f "${name}" remove >/dev/null 2>&1
    # Gentoo Linux
    rc-update del "${name}" >/dev/null 2>&1
    # Generic sysvinit
    rm -f /etc/rc.d/rc?.d/[SK]??"${name}"
    rm -f /etc/rc?.d/[SK]??"${name}"
}


terminate_proc() {
    PROC_NAME="${1}"
    SERVER_PID=`pidof $PROC_NAME 2> /dev/null`
    if [ "$SERVER_PID" != "" ]; then
        killall -TERM $PROC_NAME > /dev/null 2>&1
        sleep 2
    fi
}


maybe_run_python_bindings_installer() {
    VBOX_INSTALL_PATH="${1}"

    PYTHON=python
    if [ "`python -c 'import sys
if sys.version_info >= (2, 6):
    print \"test\"' 2> /dev/null`" != "test" ]; then
        echo  1>&2 "Python 2.6 or later not available, skipping bindings installation."
        return 1
    fi

    echo  1>&2 "Python found: $PYTHON, installing bindings..."
    # Pass install path via environment
    export VBOX_INSTALL_PATH
    $SHELL -c "cd $VBOX_INSTALL_PATH/sdk/installer && $PYTHON vboxapisetup.py install \
        --record $CONFIG_DIR/python-$CONFIG_FILES"
    cat $CONFIG_DIR/python-$CONFIG_FILES >> $CONFIG_DIR/$CONFIG_FILES
    rm $CONFIG_DIR/python-$CONFIG_FILES
    # remove files created during build
    rm -rf $VBOX_INSTALL_PATH/sdk/installer/build

    return 0
}
