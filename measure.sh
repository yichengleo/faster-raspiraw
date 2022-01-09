#!/bin/bash

cut -f1 -d, /dev/shm/tstamps.csv | sort -n | uniq -c
mv /dev/shm/tstamps.csv .
# /dev/shm/