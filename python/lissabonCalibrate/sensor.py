import iotsa
import socket
import sys
from .common import Common

class Sensor(Common):
    def __init__(self, hostname):
        Common.__init__(self, hostname)

    def post_open(self):
        self.service = self.device.rgbw
        return True

    def setInterval(self, interval):
        if interval != None:
            self.service.set('integrationInterval', interval)