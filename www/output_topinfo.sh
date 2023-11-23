#!/bin/sh

top -b -n 1 -d 0 | head -2 > /data/svm/top_output.txt
cat /data/svm/top_output.txt | head -1 > /data/svm/board_meminfo.txt
cat /data/svm/top_output.txt | head -2 | tail -1 > /data/svm/board_cpuloadinfo.txt



