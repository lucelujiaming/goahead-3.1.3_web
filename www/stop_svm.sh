#!/bin/sh

ps | grep svm | grep -v "grep" | awk '{print $1}' | sed "s/^/kill -9 /" | sh 


