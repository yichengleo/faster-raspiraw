# Faster-Raspiraw

A fast camera capturing tool for lab use exclusively.

This app is designed to capture the very moment of pitch variation in a novel design drone. It functions as directly receving raw *Bayer* format data from CSI sensors on the Raspberry Pi.

Note that: The register sets for OV5647 and IMX219 are often under NDA, which means that support can not be offered over their contents without breaking those NDAs. Anything added here by RPF/RPT has to be demonstrable as already being in the public domain, or from 3rd parties having reverse engineered how the firmware is working (e.g. by listening to the I2C communications).

Supported sensors:

	adv7282m
	imx219
	ov5647

## Usage options

Table of contents:

   $ faster-raspiraw

   faster-raspiraw Camera App 0.0.4

   -?, --help	: This help information
   -md, --mode	: Set sensor mode <mode>
   -hf, --hflip	: Set horizontal flip
   -vf, --vflip	: Set vertical flip
   -e, --ss	: Set the sensor exposure time (not calibrated units)
   -g, --gain	: Set the sensor gain code (not calibrated units)
   -o, --output	: Set the output filename
   -hd, --header	: Write the BRCM header to the output file
   -t, --timeout	: Time (in ms) before shutting down (if not specified, set to 5s)
   -sr, --saverate	: Save every Nth frame
   -b, --bitdepth	: Set output raw bit depth (8, 10, 12 or 16, if not specified, set to sensor native)
   -c, --cameranum	: Set camera number to use (0=CAM0, 1=CAM1).
   -eus, --expus	: Set the sensor exposure time in micro seconds.
   -y, --i2c	: Set the I2C bus to use.
   -r, --regs	: Change (current mode) regs
   -hi, --hinc	: Set horizontal odd/even inc reg
   -vi, --vinc	: Set vertical odd/even inc reg
   -f, --fps	: Set framerate regs
   -w, --width	: Set current mode width
   -h, --height	: Set current mode height
   -tp, --top	: Set current mode top
   -hd0, --header0	: Sets filename to write the BRCM header to
   -ts, --tstamps	: Sets filename to write timestamps to
   -emp, --empty	: Write empty output files
   $






## Dependencies

### Raspiraw
Install WiringPi and Userland_cameraraw

### Dcraw
On Buster:
```
sudo apt-get install libjasper-dev libjpeg8-dev gettext liblcms2-dev
./buildme
```
On Bullseye:
```
sudo apt-get install libjasper-dev libjpeg9-dev gettext liblcms2-dev
```



for compiling error:

```
/usr/include/jpeglib.h:29:10: fatal error: jconfig.h: No such file or directory
   29 | #include "jconfig.h"            /* widely used configuration options */
      |          ^~~~~~~~~~~
compilation terminated.
make[2]: *** [CMakeFiles/faster-raspiraw_lib.dir/build.make:76: CMakeFiles/faster-raspiraw_lib.dir/src/dcraw.c.o] Error 1
make[1]: *** [CMakeFiles/Makefile2:78: CMakeFiles/faster-raspiraw_lib.dir/all] Error 2
```

Need to manually softlink the header file to the system include folder
```
sudo ln -s /usr/include/arm-linux-gnueabihf/jconfig.h /usr/include/jconfig.h
```