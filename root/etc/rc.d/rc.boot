#!/bin/sh

result=`ps | grep 'telnetd' | grep -v grep`
if [ -z "$result" ]; then
     echo "Lujiaming starting telnetd... " > /dev/ttyS4
     # /etc/start_telnetd.sh --daemon 
     /bin/telnetd  > /dev/ttyS4
     ifconfig  > /dev/ttyS4
     ifconfig eth0  > /dev/ttyS4
     echo "Lujiaming starting telnetd over ... " > /dev/ttyS4
else
     echo "NG"
fi

