#!/usr/bin/env bash

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

kill_jack

if [ "x$1" == "x--stop" ]
then
  echo "Stop command given: not starting jack"
  exit 0
fi

echo "Execute $HOME/.jackdrc"

. /home/speakerman/.jackdrc
