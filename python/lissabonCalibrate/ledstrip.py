from .common import Common
import requests

class Ledstrip(Common):
    def __init__(self, hostname : str, protocol=None):
        Common.__init__(self, hostname, protocol=protocol)
        self.has_pps = True
    
    def post_open(self) -> bool:
        self.service = self.device.ledstrip
        self.service.transaction()
        self.service.set('isOn', True)
        self.service.set('gamma', 1.0)
        self.service.set('minLevel', 0)
        self.service.set('animation', 0)
        self.service.commit()
        # Disable automatic sleep for a few minutes, if supported
        try:
            self.device.battery.set('postponeSleep', 600000)
        except:
            self.has_pps = False
        return True

    def close(self):
        self.service.set('calibrationMode', 0)
        if self.has_pps: self.device.battery.set('postponeSleep', 0)
        pass

    def setColor(self, r : int =0, g : int =0, b : int =0, w : int =0):
        try:
            self.service.set('calibrationData', [r, g, b, w])
        except requests.exceptions.HTTPError:
            print('xxxjack retry')
            self.service.set('calibrationData', [r, g, b, w])

    def setCT(self, intensity, temperature, useRGBW=True, whiteTemperature=None, whiteBrightness=None):
        self.service.transaction()
        self.service.set('calibrationMode', 0 if useRGBW else 1)
        self.service.set('level', intensity)
        self.service.set('temperature', temperature)
        if whiteTemperature != None: self.service.set('whiteTemperature', whiteTemperature)
        if whiteBrightness != None: self.service.set('whiteBrightness', whiteBrightness)
        self.service.commit()