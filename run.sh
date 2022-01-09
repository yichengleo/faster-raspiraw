#!/bin/bash

./build/faster-raspiraw -md 7 -t 5000 -ts /dev/shm/tstamps.csv -hd0 hd0.32k -h 64 --vinc 1F --fps 660  -sr 1 -o /dev/shm/out.%04d.raw
