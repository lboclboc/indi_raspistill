# indi_mmal
indi-driver for mmal camera on RPi

NOTE: This is just for my personal development. The real driver is named indi-rpicam and has been integrated with indi-3rdparty here: https://github.com/indilib/indi-3rdparty

Just started hacking.

References
----------
- [MMAL Camera API](http://www.jvcref.com/files/PI/documentation/html/)
- [IMX477 Product Information](https://www.sony-semicon.co.jp/products/common/pdf/IMX477-AACK_Flyer.pdf)
- [INDI Development Environment](https://indilib.org/develop/developer-manual/163-setting-development-environment.html)
- [RAW12 format](https://wiki.apertus.org/index.php/RAW12)



Notes
-----
When getting a raw image (jpeg+raw), the first part of the file is a normal jpeg, in the remaining part (18711040 bytes) then
comes a 32768 bytes header starting with the text "BRCMo".
The raw image then follows which 18678272 bytes with rows 6112 bytes long: 6084 bytes (4056*1.5!!) of pixels (groups of 3),  12 blanking?? and 16 image related data.
In total 3056 rows with the last 16 rows containing other data (even in the last 28 bytes of the rows).

Pixels are ordered by | Bm | Gm | Bl+Gl |
                      | Gm | Rm | Gl+Rl |

m : MSB 8 bits, l = LSB 4 bits


INDI Debugging
--------------
To debug the code, one trick to not need to run it as a child of indiserver is to run the binary directly and then pass xml commands om the prompt.
For instance:
<getProperties version='1.7'/>

MMAL Debugging
--------------
Debug camera by adding dtdebug=1 to /boot/config.txt, reboot and run sudo vcdbg log assert

Tune up logging levels:
    vcgencmd set_logging level=3072

Log messages:
    vcdbg log msg

Set logging level for category:
List categories
    vcdbg log level <category> e w i t

or
    vcgencmd set_logging level=0x40

Enable mmal_logging:
    vcdbg syms |& grep mmal.*log
    sudo vcdbg trash 0xcf5e12c0 4   (Address of mmal_dbg_log)

Available commands to vcgencmd:
    vcgencmd commands

Displays the error log status of the various VideoCore software areas:
    vcgencmd vcos log status

More info:
- https://github.com/nezticle/RaspberryPi-BuildRoot/wiki/VideoCore-Tools
- https://elinux.org/RPI_vcgencmd_usage

Other info
----------
The camera still port supports follwing encodings:
encodings	@0xbefff29c	MMAL_FOURCC_T[25]
    [0] 	(hex) 30323449	MMAL_FOURCC_T I420
    [1] 	(hex) 3231564e	MMAL_FOURCC_T NV12
    [2] 	(hex) 32323449	MMAL_FOURCC_T I422
    [3] 	(hex) 56595559	MMAL_FOURCC_T YUYV
    [4] 	(hex) 55595659	MMAL_FOURCC_T YVYU
    [5] 	(hex) 59555956	MMAL_FOURCC_T VYUY
    [6] 	(hex) 59565955	MMAL_FOURCC_T UYVY
    [7] 	(hex) 33424752	MMAL_FOURCC_T RGB3
    [8] 	(hex) 41424752	MMAL_FOURCC_T RGBA
    [9] 	(hex) 32424752	MMAL_FOURCC_T RGB2
    [10]	(hex) 32315659	MMAL_FOURCC_T YV12
    [11]	(hex) 3132564e	MMAL_FOURCC_T NV21
    [12]	(hex) 33524742	MMAL_FOURCC_T BGR3
    [13]	(hex) 41524742	MMAL_FOURCC_T BGRA
    [14]	(hex) 5651504f	MMAL_FOURCC_T OPQV

