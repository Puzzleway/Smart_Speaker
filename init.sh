#!/bin/bash

shm_id=`ipcs | grep 3e8 | awk {'print $2'}`
#通过设置的共享内存key值（1000）来查找对应的共享内存ID，如果存在则执行删除操作。
if [ ! -z $shm_id ]; then
  ipcrm -m $shm_id
fi
#通过设置的信号量key值（1234）来查找对应的信号量ID，如果存在则执行删除操作。
sem_id=`ipcs | grep 4d2 | awk {'print $2'}`
if [ ! -z $sem_id ]; then
  ipcrm -s $sem_id
fi

rm -rf ./fifo
mkdir -p ./fifo

mkfifo ./fifo/cmd_fifo
#这是一个在启动后执行的脚本，主要功能是清理共享内存，以确保播放器能够正常运行。