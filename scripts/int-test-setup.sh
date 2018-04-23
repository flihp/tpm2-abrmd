#!/usr/bin/env bash
#;**********************************************************************;
# Copyright (c) 2017, Intel Corporation
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice,
# this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice,
# this list of conditions and the following disclaimer in the documentation
# and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
# THE POSSIBILITY OF SUCH DAMAGE.
#;**********************************************************************;
set -u
set +o nounset

# default int-test-funcs script, overridden in TEST_FUNCTIONS env variable
TEST_FUNC_LIB=${TEST_FUNC_LIB:-scripts/int-test-funcs.sh}
if [ -e ${TEST_FUNC_LIB} ]; then
    . ${TEST_FUNC_LIB}
else
    echo "Error: Unable to locate support test function library: " \
         "${TEST_FUNC_LIB}"
    exit 1
fi

usage_error ()
{
    echo "$0: $*" >&2
    print_usage >&2
    exit 2
}
print_usage ()
{
    cat <<END
Usage:
    int-simulator-setup.sh --simulator-bin=FILE --tabrmd-bin=FILE
        --tabrmd-tcti=[mssim|device] TEST-SCRIPT [TEST-SCRIPT-ARGUMENTS]
The '--tabrmd-bin' option is mandatory.
The '--tabrmd-tcti' option defaults to 'mssim' and requires the
    --simulator-bin option be provided.
END
}
SIM_BIN=""
TABRMD_BIN=""
TABRMD_TCTI="mssim"
IPC_MODE="dbus"
while test $# -gt 0; do
    case $1 in
    --help) print_usage; exit $?;;
    -i|--ipc-mode) IPC_MODE=$2; shift;;
    -i=*|--ipc-mode=*) IPC_MODE="${1#*=}";;
    -s|--simulator-bin) SIM_BIN=$2; shift;;
    -s=*|--simulator-bin=*) SIM_BIN="${1#*=}";;
    -r|--tabrmd-bin) TABRMD_BIN=$2; shift;;
    -r=*|--tabrmd-bin=*) TABRMD_BIN="${1#*=}";;
    -t|--tabrmd-tcti) TABRMD_TCTI=$2; shift;;
    -t=*|--tabrmd-tcti=*) TABRMD_TCTI="${1#*=}";;
    --) shift; break;;
    -*) usage_error "invalid option: '$1'";;
     *) break;;
    esac
    shift
done

# matrix of tabrmd-tcti & ipc-mode
#
# tabrmd-tcti: mssim
# icp-mode: dbus
#   - Parallelizable: yes
#   - Run as unpriv user: yes
#   - Must start mssim instance for TPM, we can assign a random port for the
#     mssim to bind to to allow tests to run in parallel. This requries that
#     we implement a retry loop to deal with port conflicts.
#   - tpm2-abrmd must use 'session' bus and claim unique name. Easiest way to
#     do this is to just append the random port nummber used by mssim to the
#     standard 'com.intel.tss2.Tabrmd' name.
#
# tabrmd-tcti: device
# ipc-mode: dbus
#  - Parallelizable: no
#  - Run as unpriv user: no (/dev/tpm0 is assumed to be owned by root)
#  - No need to start mssim since we're talking to /dev/tpm0 directly
#  - No need to give tpm2-abrmd a unique name, since we're running as root
#    we connect the tpm2-abrmd to the system bus and use the default name.
#
# tabrmd-tcti: mssim
# ipc-mode: tls
#  - Parallelizable: yes
#  - Run as unpriv user: yes
#  - Starup the mssim in the same way we did for mssim,dbus tuple above.
#  - tpm2-abrmd must be assigned a random port much like we do for the mssim.
#
# tabrmd-tcti: device
# ipc-mode: tls
#  - Parallelizable: no
#  - Run as unpriv user: no
#  - No need to start mssim for same reasons as device,dbus tuple above.
#  - tpm2-abrmd must be assigned a random port much like we do for the missim.

# Once option processing is done, $@ should be the name of the test executable
# followed by all of the options passed to the test executable.
TEST_BIN=$(realpath "$1")
TEST_DIR=$(dirname "$1")
TEST_NAME=$(basename "${TEST_BIN}")


# If run against the simulator we need min and max values when generating port
# numbers. We select random port values to enable parallel test execution.
PORT_MIN=1024
PORT_MAX=65534

# sanity tests
if [ ! -x "${TABRMD_BIN}" ]; then
    echo "no tarbmd binary provided or not executable"
    exit 1
fi
if [ ! -x "${TEST_BIN}" ]; then
    echo "no test binary provided or not executable"
    exit 1
fi
case "${IPC_MODE}"
in
    "dbus") ;;
    "tls") ;;
    *)
        echo "Invalid --ipc-mode, see --help."
        exit 1
        ;;
esac
case "${TABRMD_TCTI}"
in
    "mssim")
        if [ ! -x "${SIM_BIN}" ]; then
            echo "mssim TCTI requires simulator binary / executable"
            exit 1
        fi
        ;;
    "device")
        if [ `id -u` != "0" ]; then
            echo "device TCTI requires root privileges"
            exit 1
        fi
        if [ ! -z "${SIM_BIN}" ]; then
            echo "--simulator-bin and --tcti=device options are incompatible, " \
                 "chose one or the other"
            exit 1
        fi
        ;;
    *)
        echo "Invalid TABRMD_TCTI, see --help."
        exit 1
        ;;
esac

# Set up test environment and dependencies that are TCTI specific.
case "${TABRMD_TCTI}"
in
    "mssim")
        # start an instance of the simulator for the test, have it use a random port
        SIM_LOG_FILE=${TEST_BIN}_simulator.log
        SIM_PID_FILE=${TEST_BIN}_simulator.pid
        SIM_TMP_DIR=$(mktemp --directory --tmpdir=/tmp tpm_server_XXXXXX)
        BACKOFF_FACTOR=2
        BACKOFF=1
        for i in $(seq 10); do
            SIM_PORT_DATA=`shuf -i ${PORT_MIN}-${PORT_MAX} -n 1`
            SIM_PORT_CMD=$((${SIM_PORT_DATA}+1))
            echo "Starting simulator on port ${SIM_PORT_DATA}"
            simulator_start ${SIM_BIN} ${SIM_PORT_DATA} ${SIM_LOG_FILE} ${SIM_PID_FILE} ${SIM_TMP_DIR}
            sleep 1 # give daemon time to bind to ports
            PID=$(cat ${SIM_PID_FILE})
            echo "simulator PID: ${PID}";
            netstat -ltpn 2> /dev/null | grep "${PID}" | grep -q "${SIM_PORT_DATA}"
            ret_data=$?
            netstat -ltpn 2> /dev/null | grep "${PID}" | grep -q "${SIM_PORT_CMD}"
            ret_cmd=$?
            if [ \( $ret_data -eq 0 \) -a \( $ret_cmd -eq 0 \) ]; then
                echo "Simulator with PID ${PID} bound to port ${SIM_PORT_DATA} and " \
                     "${SIM_PORT_CMD} successfully.";
                break
            fi
            echo "Port conflict? Cleaning up PID: ${PID}"
            kill "${PID}"
            BACKOFF=$((${BACKOFF}*${BACKOFF_FACTOR}))
            echo "Failed to start simulator: port ${SIM_PORT_DATA} or " \
                 "${SIM_PORT_CMD} probably in use. Retrying in ${BACKOFF}."
            sleep ${BACKOFF}
            if [ $i -eq 10 ]; then
                echo "Failed to start simulator after $i tries. Giving up.";
                exit 1
            fi
        done
        TABRMD_OPTS="${TABRMD_OPTS} --tcti=${TABRMD_TCTI}:tcp://127.0.0.1:${SIM_PORT_DATA}"
        ;;
    "device")
        TABRMD_OPTS="--allow-root --tcti=device:/dev/tpm0"
        ;;
    *)
        echo "unsupported TCTI: ${TABRMD_TCTI}"
        exit 1
        ;;
esac

TABRMD_LOG_FILE=${TEST_BIN}_tabrmd.log
TABRMD_PID_FILE=${TEST_BIN}_tabrmd.pid
TABRMD_OPTS="${TABRMD_OPTS} --ipc-mode=${IPC_MODE}"
case "${IPC_MODE}"
in
    "dbus")
        TABRMD_OPTS="${TABRMD_OPTS} --session"
        TABRMD_TEST_TCTI_CONF="bus_type=session"
        case "${TABRMD_TCTI}"
        in
            "mssim") # (mssim,dbus)
                TABRMD_NAME="com.intel.tss2.Tabrmd${SIM_PORT_DATA}"
                TABRMD_OPTS="${TABRMD_OPTS} --dbus-name=${TABRMD_NAME}"
                TABRMD_TEST_TCTI_CONF="${TABRMD_TEST_TCTI_CONF},bus_name=${TABRMD_NAME}"
                tabrmd_start ${TABRMD_BIN} ${TABRMD_LOG_FILE} ${TABRMD_PID_FILE} "${TABRMD_OPTS}"
                tabrmd_start_ret=$?
                ;;
            "device") # (device,dbus)
                # do nothing, use defaults
                tabrmd_start ${TABRMD_BIN} ${TABRMD_LOG_FILE} ${TABRMD_PID_FILE} "${TABRMD_OPTS}"
                tabrmd_start_ret=$?
                ;;
        esac
        ;;
    "tls")
        TABRMD_TEST_TCTI=tabrmd-tls
        case "${TABRMD_TCTI}"
        in
            "mssim") # (mssim,tls)
                BACKOFF_FACTOR=2
                BACKOFF=1
                for i in $(seq 10); do
                    TABRMD_PORT=$(od -A n -N 2 -t u2 /dev/urandom | awk -v min=${PORT_MIN} -v max=${PORT_MAX} '{print ($1 % (max - min)) + min}')
                    echo "Starting ${TABRMD_BIN} on port ${TABRMD_PORT}"
                    tabrmd_start ${TABRMD_BIN} ${TABRMD_LOG_FILE} ${TABRMD_PID_FILE} "${TABRMD_OPTS} --socket-port=${TABRMD_PORT}"
                    sleep 1 # give daemon time to bind to ports
                    PID=$(cat ${TABRMD_PID_FILE})
                    echo "simulator PID: ${PID}";
                    netstat -ltpn 2> /dev/null | grep "${PID}" | grep -q "${TABRMD_PORT}"
                    netstat_ret=$?
                    if [ $netstat_ret -eq 0 ]; then
                        echo "${TABRMD_BIN} with PID ${PID} bound to port ${TABRMD_PORT}"
                        break
                    fi
                    echo "Port conflict? Cleaning up PID: ${PID}"
                    kill "${PID}"
                    BACKOFF=$((${BACKOFF}*${BACKOFF_FACTOR}))
                    echo "Failed to start ${TABRMD_BIN}: port ${TABRMD_PORT}. Retrying in ${BACKOFF}."
                    sleep ${BACKOFF}
                    if [ $i -eq 10 ]; then
                        echo "Failed to start ${TABRMD_BIN} after $i tries. Giving up.";
                        exit 1
                    fi
                done
                TABRMD_TEST_TCTI_CONF="host=127.0.0.1,port=${TABRMD_PORT}"
                tabrmd_start_ret=$?
                ;;
            "device") # (device,tls)
                tabrmd_start ${TABRMD_BIN} ${TABRMD_LOG_FILE} ${TABRMD_PID_FILE} "${TABRMD_OPTS}"
                tabrmd_start_ret=$?
                ;;
        esac
        ;;
esac
# start tpm2-abrmd daemon
if [ ${tabrmd_start_ret} -ne 0 ]; then
    echo "failed to start tabrmd with name ${TABRMD_NAME}"
fi

# execute the test script and capture exit code
env G_MESSAGES_DEBUG=all TABRMD_TEST_TCTI="${TABRMD_TEST_TCTI}" TABRMD_TEST_TCTI_CONF="${TABRMD_TEST_TCTI_CONF}" TABRMD_TEST_TCTI_RETRIES=10 $@
ret_test=$?

# This sleep is sadly necessary: If we kill the tabrmd w/o sleeping for a
# second after the test finishes the simulator will die too. Bug in the
# simulator?
sleep 1

# teardown tabrmd
daemon_stop ${TABRMD_PID_FILE}
ret_tabrmd=$?
rm -rf ${TABRMD_PID_FILE}

# do configuration specific tear-down
case "${TABRMD_TCTI}"
in
    # when testing against the simulator we must shut it down
    "mssim")
        # ignore exit code (it's always 143 AFAIK)
        daemon_stop ${SIM_PID_FILE}
        rm -rf ${SIM_TMP_DIR} ${SIM_PID_FILE}
        ;;
esac

# handle exit codes
if [ $ret_test -ne 0 ]; then
    echo "Execution of $@ failed: $ret_test"
    exit $ret_test
fi
if [ $ret_tabrmd -ne 0 ]; then
    echo "Execution of tabrmd failed: $ret_tabrmd"
    exit $ret_tabrmd
fi

exit 0
