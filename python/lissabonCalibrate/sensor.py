import iotsa
import socket

class Sensor:
    def __init__(self, hostname):
        hostIP = socket.gethostbyname(hostname)
        self.device = iotsa.Device(hostIP)
        self.service = self.device.rgbw
        # check that it is accessible
        _ = self.device.config.getAll()
    
    def get(self):
        return self.service.getAll()

