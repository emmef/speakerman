#!/bin/bash

warning_file="/tmp/speakerman.warn"

log_warning() {
  if [ ! -f "$warning_file" ]
  then
    echo "$*" | tee -a "$warning_file" >&2
    chmod a+rw "$warning_file"
  elif [ -w "$warning_file" ]
  then
    echo $* | tee -a "$warning_file" >&2
  else
    echo $* >&2
  fi
}


if [ -z "$1" ]
then
  log_warning "requires action argument"
  exit 0
fi

device_action=
if [[ $1 =~ --add|--remove|--get-config|--get-command ]]
then
  device_action="$1"
  shift
else
  log_warning  "Unknown action: $1"
  exit 0
fi

speakerman_user=
device_name=
device_file=

if [ -f "/etc/speakerman/config" ]
then
  speakerman_user=`cat "/etc/speakerman/config" | grep -E '^\s*user\s*\=' | tail -n 1 | sed -r 's/^\s*user\s*=\s*([-_0-9A-Za-z]+).*/\1/'`
fi
  
if [ -z "$speakerman_user" ]
then
  speakerman_user="speakerman"
fi

if ! getent passwd "$speakerman_user" >/dev/null
then
  log_warning  "Speakerman user does not exist: $speakerman_user"
  exit 0
fi  

user_directory=`eval "echo ~$speakerman_user"`

if [ -z "$user_directory" ]
then
  log_warning "User directory for speakerman user \"$speakerman_user\" not found: $user_directory"
  exit 0
fi
config_file="$user_directory/.jackdrc"
if [ ! -f "$config_file" ]
then
  log_warning "Speakerman configuration not found: $config_file"
  exit 0
fi
#device_name=`cat "$config_file" | grep -E '^\s*device-name\s*\=' | tail -n 1 | sed -r 's/^\s*device-name\s*=\s*([-_0-9A-Za-z]+).*/\1/'`
device_name=`cat "$config_file" |\
    grep -E '\s-dalsa\s|\s-d\salsa' |\
    grep -E '\s-dhw:[-_A-Za-z0-9]+\s|-d\shw:[-_A-Za-z0-9]+\s' |\
    sed -r 's/^.*-d\s*hw:([-_a-zA-Z0-9]+)\s*.*/\1/'`

if [ -z "$device_name" ]
then
  log_warning "Device name not found in configuration: $config_file"
  exit 0
fi  

message_file="/tmp/speakerman-${device_name}-${speakerman_user}"
alsa_device="/proc/asound/${device_name}"
time_stamp=`date +'%Y-%m-%d_%H_%M_%N'`

device_exists() {
  test -L "${alsa_device}" || test -f "${alsa_device}"
}

get_last_command() {
  if [ -f "$message_file" ]
  then
    if [ "x$1" == "x--stamp" ] 
    then
      cat "$message_file" | grep -E '^[-_0-9]+ (ADD|REMOVE)\s*$' | tail -n 1 | sed -r 's/^([-_0-9]+) ([A-Z]+)\s*$/\1 \2/' 2>/dev/null
    else
      cat "$message_file" | grep -E '^[-_0-9]+ (ADD|REMOVE)\s*$' | tail -n 1 | sed -r 's/^[-_0-9]+ ([A-Z]+)\s*$/\1/' 2>/dev/null
    fi
  fi
}

set_status() {
  local last_command
  local status
  
  status="$1"
  
  if [ -f "$message_file" ]
  then
    if echo "$status" | grep -E "^(ADD|REMOVE)$" >/dev/null
    then
      last_command=`get_last_command`
    fi
    line_count=`wc -l "$message_file" | sed -r 's|^([0-9]+)\s.*$|\1|'`
    if [ "0$line_count" -gt "0100" ] 
    then
      mv "$message_file" "$message_file~"
    fi
  fi
  
  if [ "x$status" == "x$last_command" ]
  then
    status="ignored $status"
  elif [ -n "$2" ]
  then
    status="$status $2"
  fi
  
  if [ ! -f "$message_file" ]
  then
    echo "$time_stamp $status" >>"$message_file"
    chmod a+w "$message_file" >/dev/null
  else
    echo "$time_stamp $status" >>"$message_file"
  fi
}

case "$device_action" in
  --add)
    if ! device_exists
    then
      set_status ignored ADD
    else 
      set_status ADD
    fi
    exit 0
  ;;
  --remove)
    if ! device_exists
    then
      set_status REMOVE
    else 
      set_status ignored REMOVE
    fi
    exit 0
  ;;
  --get-command) 
    get_last_command --stamp
  ;;
  *)
    echo "--user '$speakerman_user' device='$alsa_device' id='$device_name' messages='$message_file'"
esac

