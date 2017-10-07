#!/usr/bin/env bash

if [ -z "$1" ]
then
    echo "require output file"
    exit 1
fi

if ! echo "test" >"$1"
then
    echo "Cannot write to output file"
    exit 1
fi
OUTPUT_FILE="$1"

PARENT="$1"

URL="https://emmef.org/limiter/limiter.txt"

empty_output()
{
    if test -f "$OUTPUT_FILE"
    then
        rm "$OUTPUT_FILE"
    fi
}

empty_output

default_start="1000"
default_end="2300"
default_level="3"

exceeded_allowed()
{
#   $1 is "now" timestamp
    local now=`echo "$1" | egrep '^[0-9]{12}$'`
    if test -z "$now"
    then
        echo "No or invalid current time-stamp: $1"
        return 1
    fi
    local startstamp
    local endstamp
    local level
    local line
    local expr='([0-9]{12})-([0-9]{12}):(1|2|3|4|5|6)'
    while read line
    do
        if echo "$line" | egrep "^$expr\$" >/dev/null
        then
            startstamp=`echo "$line" | sed -r "s/^$expr/\1/"`
            endstamp=`echo "$line" | sed -r "s/^$expr/\2/"`
            level=`echo "$line" | sed -r "s/^$expr/\3/"`
            if test "$now" -ge "$startstamp" && test "$now" -le "$endstamp"
            then
                echo "$level" >"$OUTPUT_FILE"
                return
            fi
        fi
    done
}

now=`date +'%Y%m%d%H%M'`
curl -s "$URL" | exceeded_allowed "$now"
#cat "/tmp/ranges.txt" | exceeded_allowed "$now"
if ! test -f "$OUTPUT_FILE"
then
    yearmonthday=`echo "$now" | sed -r 's/^([0-9]{8})([0-9]{4})/\1/'`
    hoursminutes=`echo "$now" | sed -r 's/^([0-9]{8})([0-9]{4})/\2/'`
    if test $hoursminutes -gt "$default_start"
    then
        if test $hoursminutes -lt "$default_end"
        then
            echo "$default_level" >"$OUTPUT_FILE"
            return
        fi
    fi
fi
