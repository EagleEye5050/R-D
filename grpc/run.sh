#!/bin/bash

_usage() {
  cat << EOF

This script takes no arguments.
It will interactively prompt you to build and/or run end-to-end tests.

To run tests against the example server instead of your own, set the environment
variable USE_DEMO to a non-empty value, e.g.:
    USE_DEMO=1 ./run.sh

EOF
}

if [[ $# > 0 ]] ; then
  _usage
  exit 1
fi

BUILDDIR=build
LOGDIR=logs
RCFILE=.run_sh.rc
GOPATH=/usr/local/lib/go:$PWD/golang
export GOPATH

declare -i BASE_PORT=${BASE_PORT:-50000}
declare -i GREETER_SERVER_PORT=$((${BASE_PORT} + 51))
declare -i TRANSLATION_SERVER_PORT=$((${BASE_PORT} + 61))
declare -i GREETER_SERVER_DEMO_PORT=$((${BASE_PORT} + 71))

declare -i TEST_SERVER_PORT
if [[ -z $USE_DEMO ]] ; then
  TEST_SERVER_PORT=$GREETER_SERVER_PORT
else
  TEST_SERVER_PORT=$GREETER_SERVER_DEMO_PORT
fi

_loop() {
  PS3='
  ** Select: '
  printf '\n\n\n++++++++++++++++++++++++++++++++++++++++\n'
  select ACTION in \
      "Clean" \
      "Build only" \
      "Launch servers" \
      "Simple end-to-end test" \
      "Run Exercise Tests" \
      "Quit (kill servers)" \
      "Exit (leave servers running, if any)"
    do
      if [[ -z $ACTION ]] ; then
        ACTION=$REPLY
      fi
      case $ACTION in
        [Cc]|Clean*) _clean
          ;;
        [Bb]|Build*) _build
          ;;
        [Ll]|Launch*) _launch
          ;;
        [Ss]|Simple*) _simple
          ;;
        [Rr]|Run*) _full
          ;;
        [Qq]|Quit*)
          break
          ;;
        [EeXx]|Exit*)
          _pid_greeter=0
          _pid_greeter_demo=0
          _pid_translator=0
          break
          ;;
      esac
      REPLY=
      ACTION=
      printf '\n\n\n++++++++++++++++++++++++++++++++++++++++\n'
    done
  exit 0
}

_clean() {
  printf 'Cleaning ...\n'
  make realclean
}

_build() {
  _select_language language $language
  printf '\n(To build manually, run "make %s".)\n\n' "$language"
  mkdir -p "$BUILDDIR"
  mkdir -p "$LOGDIR"
  {
    make -C cpp translation_server exerciser
    make "$language"
  } || {
    red "Build failed."
    return 1
  }
}

_launch() {
  _kill_servers all  # Whether or not any might be running
  _build || return 1
  if [[ $1 != greeter_only ]] ; then
    build/translation_server \
      --port=${TRANSLATION_SERVER_PORT} \
      --log_dir="$LOGDIR" --logbufsecs=1 --logbuflevel=-1 \
      > "$LOGDIR/translation_server.STDOUT" \
      2> "$LOGDIR/translation_server.STDERR" & _pid_translator=$!
    disown $_pid_translator
  fi
  eval "_run_${language}"
  printf 'Waiting for servers to start...'
  if [[ $_pid_translator -gt 0 ]] ; then
    while ps -o pid,args -p $_pid_greeter,$_pid_greeter_demo,$_pid_translator | grep -q bash ; do
      sleep 1
      printf '.'
    done
  else
    while ps -o pid,args -p $_pid_greeter,$_pid_greeter_demo | grep -q bash ; do
      sleep 1
      printf '.'
    done
  fi
  declare result=0
  for i in {1..10} ; do
    if kill -0 $_pid_greeter ; then
      if nc localhost ${GREETER_SERVER_PORT} >/dev/null </dev/null ; then
        break
      else
        printf '.'
        sleep 1
      fi
    else
      red "${nl}Greeter Server not running."
      _pid_greeter=0
      result=1
      break
    fi
  done
  # Last chance:
  if nc localhost ${GREETER_SERVER_PORT} >/dev/null </dev/null ; then
    green "${nl}Greeter Server running, listening on localhost:${GREETER_SERVER_PORT}"
  else
    red "${nl}Greeter Server not listening!"
    result=1
  fi
  if [[ $_pid_greeter_demo -gt 0 ]] ; then
    if kill -0 $_pid_greeter_demo ; then
      green "Example Greeter Server running, listening on localhost:${GREETER_SERVER_DEMO_PORT}"
    else
      yellow "Example Greeter Server not running."
      _pid_greeter_demo=0
      result=1
    fi
  fi
  if [[ $_pid_translator -gt 0 ]] ; then
    if kill -0 $_pid_translator ; then
      green "Translator Server running, listening on localhost:${TRANSLATION_SERVER_PORT}"
    else
      red "Translator Server not running."
      _pid_greeter_demo=0
      result=1
    fi
    if [[ $result == 0 ]] ; then
      [[ $_pid_greeter_demo -gt 0 ]] &&
        ps -o pid,args -p $_pid_greeter,$_pid_greeter_demo,$_pid_translator ||
        ps -o pid,args -p $_pid_greeter,$_pid_translator
      return $?
    else
      return $result
    fi
  else
    [[ $_pid_greeter_demo -gt 0 ]] &&
      ps -o pid,args -p $_pid_greeter,$_pid_greeter_demo ||
      ps -o pid,args -p $_pid_greeter
    return $?
  fi
}

_simple() {
  _launch || return 1
  case $language in
    cpp) client_cmd="build/greeter_client --greeter_server=localhost:${TEST_SERVER_PORT} --log_dir=$LOGDIR" ;;
    python) client_cmd="build/greeter_client.py --greeter_server=localhost:${TEST_SERVER_PORT} --log_dir=$LOGDIR" ;;
    golang) client_cmd="build/go_greeter_client --greeter_server=localhost:${TEST_SERVER_PORT}" ;;
    java) client_cmd="java -jar build/greeter-client.jar --greeter_server=localhost:${TEST_SERVER_PORT}" ;;
    *) red "Cannot test $language"
       return 1
       ;;
  esac
  declare cmd="${client_cmd} --user='SREcon attendee'"
  printf '\n\nRunning:\n  %s\n' "${cmd}"
  eval "${cmd}" \
    2> "$LOGDIR/greeter_client.STDERR" | tee "$LOGDIR/greeter_client.STDOUT"
  result=$?
  if [[ $result == 0 ]] ; then
    grep -q 'SREcon attendee' "${LOGDIR}/greeter_client.STDOUT"
    result=$?
  fi
  if [[ $result == 0 ]] ; then
    green "${nl}SUCCESS!"
  else
    red "${nl}Test failed!"
    printf 'Expected the output to contain "SREcon attendee".\n'
    printf 'Logs are in the logs/ subdirectory\n\n'
  fi
}

_full() {
  printf '\n\n\n++++++++++++++++++++++++++++++++++++++++\nWhich Exercise?\n'
  select EX in \
      "Add a new gRPC backend to the Greeter" \
      "Client Deadlines and Server Timeouts" \
      "Backend Disappears" \
      "Be cheap and be helpful" \
      "Bi-directional Streaming, Client and Server" \
      "Streaming Timeouts and Other Failures" \
      "Quit"
    do
      if [[ -z $EX ]] ; then
        EX=$REPLY
      fi
      case $EX in
        Add*)          _exercise 1 ;;
        *Deadlines*)   _exercise 2 ;;
        *Disappears*)  _exercise 3 ;;
        *cheap*)       _exercise 4 ;;
        Bi-di*)        _exercise 5 ;;
        Streaming*)    _exercise 6 ;;
        [QqXx]|Quit)  break ;;
      esac
    done
  return
}

_run_cpp() {
  build/greeter_server_demo \
    --port=${GREETER_SERVER_DEMO_PORT} \
    --translation_server=localhost:${TRANSLATION_SERVER_PORT} \
    --log_dir=$LOGDIR --logbufsecs=1 --logbuflevel=-1 \
    > "$LOGDIR/greeter_server_demo.STDOUT" \
    2> "$LOGDIR/greeter_server_demo.STDERR" & _pid_greeter_demo=$!
  disown $_pid_greeter_demo
  build/greeter_server \
    --port=${GREETER_SERVER_PORT} \
    --translation_server=localhost:${TRANSLATION_SERVER_PORT} \
    --log_dir=$LOGDIR --logbufsecs=1 --logbuflevel=-1 \
    > "$LOGDIR/greeter_server.STDOUT" \
    2> "$LOGDIR/greeter_server.STDERR" & _pid_greeter=$!
  disown $_pid_greeter
}

_run_python() {
  build/greeter_server_demo.py \
    --port=${GREETER_SERVER_DEMO_PORT} \
    --translation_server=localhost:${TRANSLATION_SERVER_PORT} \
    --log_dir=$LOGDIR \
    > "$LOGDIR/greeter_server_demo.STDOUT" \
    2> "$LOGDIR/greeter_server_demo.STDERR" & _pid_greeter_demo=$!
  disown $_pid_greeter_demo
  build/greeter_server.py \
    --port=${GREETER_SERVER_PORT} \
    --translation_server=localhost:${TRANSLATION_SERVER_PORT} \
    --log_dir=$LOGDIR \
    > "$LOGDIR/greeter_server.STDOUT" \
    2> "$LOGDIR/greeter_server.STDERR" & _pid_greeter=$!
  disown $_pid_greeter
}

_run_golang() {
  build/go_greeter_server_demo \
    --port=${GREETER_SERVER_DEMO_PORT} \
    --translation_server=localhost:${TRANSLATION_SERVER_PORT} \
    > "$LOGDIR/greeter_server_demo.STDOUT" \
    2> "$LOGDIR/greeter_server_demo.STDERR" & _pid_greeter_demo=$!
  disown $_pid_greeter_demo
  build/go_greeter_server \
    --port=${GREETER_SERVER_PORT} \
    --translation_server=localhost:${TRANSLATION_SERVER_PORT} \
    > "$LOGDIR/greeter_server.STDOUT" \
    2> "$LOGDIR/greeter_server.STDERR" & _pid_greeter=$!
  disown $_pid_greeter
}

_run_java() {
  java -jar build/greeter-server-demo.jar \
    --port=${GREETER_SERVER_DEMO_PORT} \
    --translation_server=localhost:${TRANSLATION_SERVER_PORT} \
    > "$LOGDIR/greeter_server_demo.STDOUT" \
    2> "$LOGDIR/greeter_server_demo.STDERR" & _pid_greeter_demo=$!
  disown $_pid_greeter_demo
  java -jar build/greeter-server.jar \
    --port=${GREETER_SERVER_PORT} \
    --translation_server=localhost:${TRANSLATION_SERVER_PORT} \
    > "$LOGDIR/greeter_server.STDOUT" \
    2> "$LOGDIR/greeter_server.STDERR" & _pid_greeter=$!
  disown $_pid_greeter
}

_exercise() {
  declare -A launch_arg
  launch_arg=([3]='greeter_only')
  _launch ${launch_arg[$1]} || return 1
  echo
  build/exerciser --exercise "$1" \
    --greeter_server=localhost:${TEST_SERVER_PORT} \
    --translation_server=localhost:${TRANSLATION_SERVER_PORT} \
    --log_dir="$LOGDIR" --logbufsecs=1 --logbuflevel=-1
  declare result=$?
  if [[ $result == 0 ]] ; then
    green "${nl}SUCCESS!"
  else
    printf '\n%sTest failed!%s\n' "${col_RED}" "${col_NORMAL}"
    printf 'Logs are in the logs/ subdirectory\n\n'
  fi
}

_kill_servers() {
  # Any parameter will cause the killall to be run even if no PID is saved.
  # This is to allow cleaning up after a restart.
  declare _all=$1
  if [[ ${_pid_greeter} > 0 ]] ; then
    kill ${_pid_greeter} 2>/dev/null
    _all=${_all}x
    _pid_greeter=0
  fi
  if [[ ${_pid_greeter_demo} > 0 ]] ; then
    kill ${_pid_greeter_demo} 2>/dev/null
    _all=${_all}x
    _pid_greeter_demo=0
  fi
  if [[ ${_pid_translator} > 0 ]] ; then
    kill ${_pid_translator} 2>/dev/null
    _all=${_all}x
    _pid_translator=0
  fi
  if [[ -n $_all ]] ; then
    # python's argv[0] is "python", so killall(1) wouldn't find those
    declare pids=$(ps o pid,args | awk '/.*(greeter|translation)_server.*/ { print $1 }')
    [[ -n $pids ]] && kill $pids >/dev/null 2>&1
  fi
  return 0
}

_select_language() {
  # args:
  #   $1: reply variable name
  #   $2 default value (optional)
  declare msg="${nl}${nl}Which language (cpp, java, go, python)? "
  [[ -n $2 ]] && msg="${msg} [$2] "
  declare lang_
  while [[ -z $lang_ ]] ; do
    read -p "${msg}"
    case "${REPLY,,}" in
      c*) lang_=cpp ;;
      j*) lang_=java ;;
      g*) lang_=golang ;;
      p*) lang_=python ;;
      '') lang_="$2" ;;
    esac
  done
  eval $1=$lang_
  # Save selection for the future:
  declare _f=$(mktemp)
  grep -v "^${1}=" ${RCFILE} 2>/dev/null > ${_f}
  (cat ${_f} ; printf '%s=%s\n' "${1}" "${lang_}" ) > ${RCFILE}
  rm ${_f} 2>/dev/null
}

declare hr='--------------------------------------------------------------------------------'
declare nl='
'
declare -i _pid_greeter=0 _pid_greeter_demo=0 _pid_translator=0

declare col_GREEN="$(tput bold)$(tput setf 2)"
declare col_RED="$(tput bold)$(tput setf 4)"
declare col_YELLOW="$(tput bold)$(tput setf 6)"
declare col_NORMAL="$(tput sgr0)"

red() {
  printf '%s%s%s\n' "${col_RED}" "$1" "${col_NORMAL}"
}

yellow() {
  printf '%s%s%s\n' "${col_YELLOW}" "$1" "${col_NORMAL}"
}

green() {
  printf '%s%s%s\n' "${col_GREEN}" "$1" "${col_NORMAL}"
}

[[ -f .run_sh.rc ]] && source .run_sh.rc

trap _kill_servers EXIT
_loop
