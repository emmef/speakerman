#!/bin/bash

warning_file="/tmp/speakerman.warn"
time_stamp=`date +'%Y-%m-%d_%H:%M:%S.%N'`

log_warning() {
  if [ -w "$warning_file" ]
  then
    echo "$time_stamp # $*" | tee -a "$warning_file" >&2
  else
    echo "$time_stamp # $*" >&2
  fi
}

if [ -z "$1" ]
then
  log_warning "Requires action argument"
  exit 0
fi

device_action=
device_number=
if [[ $1 =~ --add|--remove ]]
then
  device_action="$1"
  shift
  if [ -n "$1" ] && [[ $1 =~ ^[0-9]$ ]]
  then
    device_number="$1"
    shift
  else
    log_warning "Require device number argument for --add and --remove."
    exit 0
  fi
elif [[ $1 =~ --get-config|--get-command ]]
then
  device_action="$1"
  shift
else
  log_warning "Unknown action: $1"
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

device_name=$(cat "$config_file" |\
    grep -E '\s-dalsa\s|\s-d\salsa' |\
    grep -E '\s-dhw:[-_A-Za-z0-9]+\s|-d\shw:[-_A-Za-z0-9]+\s' |\
    sed -r 's/^.*-d\s*hw:([-_a-zA-Z0-9]+)\s*.*/\1/')

if [ -z "$device_name" ]
then
  log_warning "Device name not found in configuration: $config_file"
  exit 0
fi  

alsa_device="/proc/asound/${device_name}"
message_file="/tmp/speakerman-${device_name}"

get_last_command_line() {
  local commands

  if [ -n "$1" ]
  then
    commands="$1"
  else
    commands="(ADD|REMOVE)"
  fi
  if [ -f "$message_file" ]
  then
    grep -E "^[-.:_0-9]+ ${commands} ([0-9]+)\$" "$message_file"| tail -n 1
  fi
}

last_command_line=$(get_last_command_line)
last_stamp=$(echo "$last_command_line" | sed -r 's/^([-._:0-9]+) ([A-Z]+) ([0-9]+)$/\1/' 2>/dev/null)
last_command=$(echo "$last_command_line" | sed -r 's/^([-._:0-9]+) ([A-Z]+) ([0-9]+)$/\2/' 2>/dev/null)

last_added_device=$(get_last_command_line "ADD" | sed -r 's/^[-._:0-9]+ [A-Z]+ ([0-9]+)$/\1/' 2>/dev/null)

device_state_message=

device_state() {

  device_state_message=

  case "$1" in
    ADD)
      if ! [ -L "${alsa_device}" ]
      then
        device_state_message="ALSA device $alsa_device does not exist"
        return 1
      fi
      local link_number
      link_number=$(readlink "${alsa_device}")
      if [ "$link_number" != "card${device_number}" ]
      then
        device_state_message="ALSA device $alsa_device ($link_number) does not match event device number $device_number"
        return 1
      fi
      return 0
      ;;
    REMOVE)
      if [ "$last_added_device" != "$device_number" ]
      then
        device_state_message="Event device number $device_number does not match last number $last_added_device for ALSA device $alsa_device"
        return 1
      fi
      if [ -L "${alsa_device}" ]
      then
        device_state_message="Inconsistent remove: ALSA device $alsa_device still exists"
        return 1
      fi
      return 0
      ;;
  esac
  device_state_message="not intended device or inconsistent state (last-added-device=$last_added_device event-device-num=$device_number action=$1)"
  return 1
}

write_status() {
  local output_file
  local max_size
  local message

  if [ "x$1" == "xignore" ]
  then
    shift
    output_file="$message_file.ignored";
    max_size="1000000"
    message="Ignored $* : $device_state_message"
  elif [ "x$1" == "x$last_command" ]
  then
    output_file="$message_file.ignored";
    max_size="1000000"
    message="Ignored $* : duplicate command"
  else
    output_file="$message_file"
    max_size="10000"
    message="$*"
  fi

  if [ -f "$output_file" ]
  then
    local file_size
    files_size=$(stat -c "%s" "$output_file")
    if [ "0$files_size" -gt "0100000" ]
    then
      mv "$output_file" "$output_file~"
    fi
  fi

  echo "$time_stamp $message" >>"$output_file"
}

case "$device_action" in
  --add)
    if device_state ADD
    then
      write_status ADD $device_number
    else
      write_status ignore ADD
    fi
    exit 0
  ;;
  --remove)
    if device_state REMOVE
    then
      write_status REMOVE $device_number
    else
      write_status ignore REMOVE
    fi
    exit 0
  ;;
  --get-command) 
    echo "$last_stamp $last_command"
  ;;
  *)
    echo "--user '$speakerman_user' device='$alsa_device' id='$device_name' messages='$message_file'"
esac

