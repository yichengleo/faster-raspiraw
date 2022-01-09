# Faster-Raspiraw

A fast camera capturing tool for lab use exclusively.

This app is designed to capture the very moment of pitch variation in a novel design drone. It functions as directly receving raw data from CSI sensors on the Raspberry Pi.

Note that: The register sets for OV5647 and IMX219 are often under NDA, which means that support can not be offered over their contents without breaking those NDAs. Anything added here by RPF/RPT has to be demonstrable as already being in the public domain, or from 3rd parties having reverse engineered how the firmware is working (e.g. by listening to the I2C communications).