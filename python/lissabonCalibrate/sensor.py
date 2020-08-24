import iotsa
import socket
import sys

class Sensor:
    def __init__(self, hostname):
        self.hostname = hostname
        self.device = None
        self.service = None

    def open(self):
        try:
            hostIP = socket.gethostbyname(self.hostname)
        except socket.gaierror as e:
            print(f'{self.hostname}: {e}', file=sys.stderr)
            return False
        # check that it is accessible
        self.device = iotsa.Device(hostIP)
        _ = self.device.config.getAll()
        self.service = self.device.rgbw
        return True

    def close(self):
        pass
    
    def get(self):
        return self.service.getAll()

