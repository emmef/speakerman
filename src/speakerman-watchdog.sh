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
    local expr_nodashes='([0-9]{12})( *- *)([0-9]{12})( *: *)(1|2|3|4|5|6)'
    local expr_dashes='([0-9]{4}-[0-9]{2}-[0-9]{2}(T|_| )[0-9]{2}:[0-9]{2})( *- *)([0-9]{4}-[0-9]{2}-[0-9]{2}(T|_| )[0-9]{2}:[0-9]{2})( *: *)(1|2|3|4|5|6)'
    while read line
    do
        if echo "$line" | egrep "^$expr_nodashes\$" >/dev/null
        then
            startstamp=`echo "$line" | sed -r "s/^$expr_nodashes/\1/"`
            endstamp=`echo "$line" | sed -r "s/^$expr_nodashes/\3/"`
            level=`echo "$line" | sed -r "s/^$expr_nodashes/\5/"`
            echo "START $startstamp END $endstamp LEVEL $level"
        elif echo "$line" | egrep "^$expr_dashes\$" >/dev/null
        then
            startstamp=`echo "$line" | sed -r "s/^$expr_dashes/\1/" | tr -d 'T: \-_'`
            endstamp=`echo "$line" | sed -r "s/^$expr_dashes/\4/" | tr -d 'T: \-_'`
            level=`echo "$line" | sed -r "s/^$expr_dashes/\7/"`
            echo "START $startstamp END $endstamp LEVEL $level"
        else
            startstamp=
            endstamp=
            level=
        fi
        if echo "$startstamp-$endstamp:$level" | egrep '^[0-9]{12}-[0-9]{12}:(1|2|3|4|5|6)$' >/dev/null
        then
            if test "$now" -ge "$startstamp" && test "$now" -le "$endstamp"
            then
                echo "$level" >"$OUTPUT_FILE"
                exit
            fi
        fi
    done
}

now=`date +'%Y%m%d%H%M'`
curl -s "$URL" | exceeded_allowed "$now"
if ! test -f "$OUTPUT_FILE"
then
    echo "1" >"$OUTPUT_FILE"
fi