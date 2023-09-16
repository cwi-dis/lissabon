import socket
import sys
from typing import Optional
import iotsa

class Common:
    """Common code for RGBW sensor and ledstrip iotsa modules"""
    def __init__(self, hostname : str, protocol : Optional[str]=None):
        self.hostname = hostname
        self.protocol : Optional[str] = protocol
        self.device : Optional[iotsa.IotsaDevice] = None
        self.service : Optional[iotsa.IotsaEndpoint] = None

    def open(self) -> bool:
        if not self.pre_open(): return False
        self.device = iotsa.Device(self.hostname, protocol=self.protocol)
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

