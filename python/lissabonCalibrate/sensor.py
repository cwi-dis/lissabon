import iotsaControl.api
import socket

class Sensor:
    def __init__(self, hostname):
        hostIP = socket.gethostbyname(hostname)
        self.device = iotsaControl.api.IotsaDevice(hostIP)
        self.service = self.device.getApi('rgbw')
        # check that it is accessible
        _ = self.device.config.getAll()
    
    def get(self):
        return self.service.getAll()

