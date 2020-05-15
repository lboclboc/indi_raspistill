# indi_mmal
indi-driver for mmal camera on RPi

Just started hacking.

References
----------
- [MMAL Camera API](http://www.jvcref.com/files/PI/documentation/html/)
- [IMX477 Product Information](https://www.sony-semicon.co.jp/products/common/pdf/IMX477-AACK_Flyer.pdf)
- [INDI Development Environment](https://indilib.org/develop/developer-manual/163-setting-development-environment.html)



Notes
-----
When getting a raw image (jpeg+raw), the first part of the file is a normal jpeg, in the remaining part (18711040 bytes) then
comes a 32768 bytes header starting with the text "BRCM".
The raw image then follows which is rows of 6112 pixes (ordered by 3 bytes each) x 3056 rows each line is divided by 3 fields.
