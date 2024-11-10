#!/bin/bash
./build/faster-raspiraw -md 7 -t 1000 -ts /dev/shm/tstamps.csv -h 64 -w 640 --vinc 1F --fps 660  -sr 1 -o /dev/shm/out.%04d.raw
