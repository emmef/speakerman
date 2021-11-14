#!/bin/bash

action_data=
last_stamp=
last_action=
user=
device=
current_stamp=
current_action=
jack_pid=

set_current_values() {
  action_data=`/usr/local/bin/speakerman-device-action.sh --get-command`
  
  current_stamp=`echo "$action_data" | sed -r 's/^([-._:0-9]+) ([A-Z]+)$/\1/'`
  current_action=`echo "$action_data" | sed -r 's/^([-._:0-9]+) ([A-Z]+)$/\2/'`
}

jack_running() {
  pidof jackd >/dev/null
}

kill_jack() {
  echo "Terminate old instance of jack -- if any"
  local attempt
  attempt="."
  
  while jack_running
  do 
    if [ "$attempt" == "." ] 
    then
      echo "Terminate jack"
      killall -TERM jackd
    else
      sleep 1
    fi
    attempt=".$attempt"
    if [ "$attempt" == "....." ]
    then
      echo "Kill jack"
      killall -KILL jackd
      break
    fi
  done
}


start_jackd() {
  local jack_command
  
  kill_jack
  jack_command=`cat ~/.jackdrc`
  $jack_command & 
  jack_pid="$!"
  disown
  echo "JAck started with PID $jack_pid"
}

while sleep 2
do
  set_current_values 
  if [ "$current_stamp" == "$last_stamp" ]
  then
    continue
  fi
  case "$current_action" in
    ADD) 
      start_jackd
      ;;
    REMOVE)
      echo "Kill speakerman"
      killall -TERM speakerman
      if [ -n "$jack_pid" ]
      then
        echo "Kill jackd $jack_pid"
        kill -TERM $jack_pid
        jack_pid=
      fi
    ;;
    *) 
      echo "Idle (data=$action_data; stamp=$current_stamp; action=$current_action"
  esac
  last_stamp="$current_stamp"
done 


