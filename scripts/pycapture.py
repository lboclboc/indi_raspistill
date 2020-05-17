#!/usr/bin/env python3

import sys, time, logging
import PyIndi

DEVICE_NAME = "MMAL Device"
#DEVICE_NAME = "CCD Simulator"

class IndiClient(PyIndi.BaseClient):
    def __init__(self):
        super(IndiClient, self).__init__()
        self.exposure = None
        self.device = None
        self.done = False
        self.logger = logging.getLogger('PyQtIndi.IndiClient')
        self.logger.info('creating an instance of PyQtIndi.IndiClient')

    def newDevice(self, d):
        self.logger.info ("new Device '%s'" % d.getDeviceName())
        if d.getDeviceName() == DEVICE_NAME:
            self.logger.info("Set new device %s!" % DEVICE_NAME)
            # save reference to the device in member variable
            self.device = d

    def newProperty(self, p):
        self.logger.info("%s: newProperty(%s)=%s(%s)" % (p.getDeviceName(), p.getName(), p.getNumber(), p.getType()))
        if self.device is not None and p.getDeviceName() == self.device.getDeviceName():
            if p.getName() == "CONNECTION":
                self.logger.info("Got property CONNECTION for %s!" % DEVICE_NAME)
                # connect to device
                self.connectDevice(self.device.getDeviceName())
                # set BLOB mode to BLOB_ALSO
                self.setBLOBMode(1, self.device.getDeviceName(), None)

            elif p.getName() == "CCD_EXPOSURE":
                self.exposure = self.device.getNumber("CCD_EXPOSURE")
            else:
                print("Property : " + p.getName())

    def removeProperty(self, p):
        self.logger.info("%s: removeProperty(%s)=%s(%s)" % (p.getDeviceName(), p.getName(), p.getNumber(), p.getType()))

    def newBLOB(self, bp):
        self.logger.info("new BLOB "+ bp.name)
        # get image data
        img = bp.getblobdata()
        # open a file and save buffer to disk
        with open("frame.fit", "wb") as f:
            f.write(img)
        self.done = True

    def newSwitch(self, svp):
        self.logger.info ("new Switch "+ svp.name + " for device "+ svp.device)

    def newNumber(self, nvp):
        self.logger.info("new Number "+ nvp.name + " for device "+ nvp.device + ", value " + str(nvp.np.value))

    def newText(self, tvp):
        self.logger.info("new Text "+ tvp.name + " for device "+ tvp.device)

    def newLight(self, lvp):
        self.logger.info("new Light "+ lvp.name + " for device "+ lvp.device)

    def newMessage(self, d, m):
        self.logger.info("newMessage(%s)=%s" % (d.getDeviceName(), d.messageQueue(m)))

    def serverConnected(self):
        print("Server connected ("+self.getHost()+":"+str(self.getPort())+")")
        self.connected = True

    def serverDisconnected(self, code):
        self.logger.info("Server disconnected (exit code = "+str(code)+","+str(self.getHost())+":"+str(self.getPort())+")")
        # set connected to False
        self.connected = False

    def takeExposure(self):
        self.logger.info("<<<<<<<< Exposure >>>>>>>>>")
        #get current exposure time
        self.exposure[0].value = 2
        # send new exposure time to server/device
        self.sendNewNumber(self.exposure)

logging.basicConfig(format='%(asctime)s %(message)s', level=logging.INFO)

if __name__ == '__main__':
    # Connect to indi-server
    indiclient = IndiClient()
    indiclient.setServer("localhost", 7624)
    if (not(indiclient.connectServer())):
         print("No indiserver running on " + indiclient.getHost() + ":" + str(indiclient.getPort()) + " - Try to run")
         print("  indiserver indi_simulator_telescope indi_simulator_ccd")
         sys.exit(1)

    while indiclient.exposure is None:
        time.sleep(1)

    print("Taking picture")
    indiclient.takeExposure()
    while not indiclient.done:
        time.sleep(1)

    indiclient.disconnectServer()
