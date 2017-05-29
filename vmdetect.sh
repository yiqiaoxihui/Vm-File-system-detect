#!/bin/bash
echo "begin to run vm detect......"
run_date=3
now_date=0
#echo ${now_date}
while true
do
	now_date=$(date +%H)
	#echo $now_date
	if [ $now_date == $run_date ]
	then
		/home/heaven/Documents/C/test/bin/Debug/test
	fi
		
done
