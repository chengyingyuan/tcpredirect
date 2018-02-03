#!/bin/bash

export TCPREDIRECT_KEY=":secret"

#./tcpredirect -t 60:60 -k :secret 127.0.0.1:5400:127.0.0.1:5401
exec ./tcpredirect -t 60:60 127.0.0.1:5400:127.0.0.1:5401

