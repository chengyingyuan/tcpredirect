#!/bin/bash
#

MYDIR=$(realpath $(dirname $0))
export TCPREDIRECT_KEY=":secret"

$MYDIR/tcpredirect -t 60:60 :5400:178.128.25.176:8080 >/dev/null 2>&1 &
echo $! > $MYDIR/tcpredirect.pid


