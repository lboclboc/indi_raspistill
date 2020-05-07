#!/usr/bin/env python3
"""
Python based indi driver for RPI high quality camera.
API ref.: https://sourceforge.net/p/pyindi-client/code/HEAD/tree/trunk/pip/pyindi-client/
"""
import PyIndi
from picamera import PiCamera
import time
import logging
import sys

def camera_stuff():
    camera = PiCamera()

    camera.start_preview()
    time.sleep(5)
    camera.stop_preview()

class IndiClient(PyIndi.BaseClient):
    def __init__(self):
        super(IndiClient, self).__init__()
        self.logger = logging.getLogger('IndiClient')
        self.logger.info('creating an instance of IndiClient')
    def newDevice(self, d):
        self.logger.info("new device " + d.getDeviceName())
    def newProperty(self, p):
        self.logger.info("new property "+ p.getName() + " for device "+ p.getDeviceName())
    def removeProperty(self, p):
        self.logger.info("remove property "+ p.getName() + " for device "+ p.getDeviceName())
    def newBLOB(self, bp):
        self.logger.info("new BLOB "+ bp.name.decode())
    def newSwitch(self, svp):
        self.logger.info ("new Switch "+ svp.name.decode() + " for device "+ svp.device.decode())
    def newNumber(self, nvp):
        self.logger.info("new Number "+ nvp.name.decode() + " for device "+ nvp.device.decode())
    def newText(self, tvp):
        self.logger.info("new Text "+ tvp.name.decode() + " for device "+ tvp.device.decode())
    def newLight(self, lvp):
        self.logger.info("new Light "+ lvp.name.decode() + " for device "+ lvp.device.decode())
    def newMessage(self, d, m):
        self.logger.info("new Message "+ d.messageQueue(m))
    def serverConnected(self):
        self.logger.info("Server connected ("+self.getHost()+":"+str(self.getPort())+")")
    def serverDisconnected(self, code):
        self.logger.info("Server disconnected (exit code = "+str(code)+","+str(self.getHost())+":"+str(self.getPort())+")")
        
        
if __name__ == '__main__':
    logging.basicConfig(format='%(asctime)s %(message)s', level=logging.INFO)

    # Create an instance of the IndiClient class and initialize its host/port members
    indiclient=IndiClient()    # Connect to server
    
    print("Connecting and waiting 1 sec")
    if (not(indiclient.connectServer())):
         print("No indiserver running on "+indiclient.getHost()+":"+str(indiclient.getPort())+" - Try to run")
         print("  indiserver indi_simulator_telescope indi_simulator_ccd")
         sys.exit(1)
    time.sleep(1)

    # Print list of devices. The list is obtained from the wrapper function getDevices as indiclient is an instance
    # of PyIndi.BaseClient and the original C++ array is mapped to a Python List. Each device in this list is an
    # instance of PyIndi.BaseDevice, so we use getDeviceName to print its actual name.
    print("List of devices")
    dl=indiclient.getDevices()
    for dev in dl:
        print(dev.getDeviceName())    
    
