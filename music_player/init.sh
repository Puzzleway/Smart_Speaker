#!/bin/bash

shmid=`ipcs | grep 3e8 | awk {'print $2'}`

if [ ! -z $shmid ]; then
	ipcrm -m $shmid
fi

semid=`ipcs | grep 4d2 | awk {'print $2'}`

if [ ! -z $semid ]; then
	ipcrm -s $semid
fi


rm -rf /home/fifo

mkdir -p /home/fifo

mkfifo /home/fifo/cmd_fifo
mkfifo /home/fifo/asr_fifo
mkfifo /home/fifo/tts_fifo

