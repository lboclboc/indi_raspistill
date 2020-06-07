#!/bin/bash

indi_setprop "MMAL Device.CONNECTION.CONNECT=On"
indi_setprop "MMAL Device.CCD_EXPOSURE.CCD_EXPOSURE_VALUE=2"
indi_getprop -m
