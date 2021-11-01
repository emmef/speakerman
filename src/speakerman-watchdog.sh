#!/usr/bin/env bash


checking=

if [ "$1" == "--check" ]
then
  checking="yes"
elif [ -z "$1" ]
then
  echo "require output file"
  exit 1
elif ! echo "test" >"$1";
then
  echo "Cannot write to output file"
  exit 1
else
  output_file="$1"
fi

URL="https://emmef.org/limiter/limiter.txt"

exceeded_allowed() {
  #   $1 is "now" timestamp
  local now
  now=$(echo "$1" | grep -E '^[0-9]{12}$')
  if test -z "$now"; then
    echo "No or invalid current time-stamp: $1"
    return 1
  fi
  if [ -n "$checking" ]
  then
    echo "NOW = $now"
  fi
  local startstamp
  local endstamp
  local level
  local line
  local expr_nodashes='^([0-9]{12})( *- *)([0-9]{12})( *: *)(1|2|3|4|5|6)(| .*)$'
  local expr_dashes='^([0-9]{4}-[0-9]{2}-[0-9]{2}(T|_| )[0-9]{2}:[0-9]{2})( *- *)([0-9]{4}-[0-9]{2}-[0-9]{2}(T|_| )[0-9]{2}:[0-9]{2})( *: *)(1|2|3|4|5|6)(| .*)$'
  while read -r line; do
    line=$(echo "$line" | sed -r 's/^\s+(.*)$/\1/' | sed -r 's/^([^#]*)(#.*)*$/\1/')
    if echo "$line" | grep -E "$expr_nodashes" >/dev/null; then
      startstamp=$(echo "$line" | sed -r "s/$expr_nodashes/\1/")
      endstamp=$(echo "$line" | sed -r "s/$expr_nodashes/\3/")
      level=$(echo "$line" | sed -r "s/$expr_nodashes/\5/")
    elif echo "$line" | grep -E "$expr_dashes" >/dev/null; then
      startstamp=$(echo "$line" | sed -r "s/$expr_dashes/\1/" | tr -d 'T: \-_')
      endstamp=$(echo "$line" | sed -r "s/$expr_dashes/\4/" | tr -d 'T: \-_')
      level=$(echo "$line" | sed -r "s/$expr_dashes/\7/")
    else
      startstamp=
      endstamp=
      level=
    fi
    if echo "$startstamp-$endstamp:$level" | grep -E '^[0-9]{12}-[0-9]{12}:(1|2|3|4|5|6)$' >/dev/null
    then
      if test "$now" -ge "$startstamp" && test "$now" -le "$endstamp"
      then
        if [ -n "$checking" ]
        then
          echo "RANGE $startstamp-$endstamp:$level ACTIVE"
        else
          echo "$level" >"$output_file"
          exit
        fi
      elif [ -n "$checking" ]
      then
        echo "RANGE $startstamp-$endstamp:$level (INACTIVE)"
      fi
    fi
  done
}

now=$(date +'%Y%m%d%H%M')
curl -s "$URL" | exceeded_allowed "$now"
if [ -z "$checking" ] && ! test -f "$output_file"
then
  echo "1" >"$output_file"
fi
