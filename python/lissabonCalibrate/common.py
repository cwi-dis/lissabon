import socket
import sys
from typing import Optional
import iotsa

class Common:
    """Common code for RGBW sensor and ledstrip iotsa modules"""
    def __init__(self, hostname : str):
        self.hostname = hostname
        self.device : Optional[iotsa.IotsaDevice] = None
        self.service : Optional[iotsa.IotsaEndpoint] = None

    def open(self) -> bool:
        if not self.pre_open(): return False
        try:
            hostIP = socket.gethostbyname(self.hostname)
        except socket.gaierror as e:
            print(f'{self.hostname}: {e}', file=sys.stderr)
            return False
        # check that it is accessible
        self.device = iotsa.Device(hostIP)
        _ = self.device.config.getAll()
        return self.post_open()

    def pre_open(self) -> bool:
        return True

    def post_open(self) -> bool:
        return True

    def pre_close(self) -> bool:
        pass

    def post_close(self) -> bool:
        pass

    def close(self):
        self.pre_close()
        pass
        self.post_close()
        self.device = None
        self.service = None
    
    def get(self):
        return self.service.getAll()

