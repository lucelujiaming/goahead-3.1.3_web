#!/bin/sh

top_count=0
echo "top_output 11111"
while true; do 
    if [ $top_count -gt 9 ]; then                         
        top -b -n 1 -d 0 | head -2 > /data/svm/top_output.txt
        echo "top_output"
        top_count=0
    fi                                                   
    top_count=`expr $top_count + 1`
    sleep 1                                                          
    echo "top_output 2222"
done 
echo "top_output 333"
echo "top_output 333"
echo "top_output 333"
echo "top_output 333"



