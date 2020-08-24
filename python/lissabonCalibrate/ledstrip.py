from .common import Common

class Ledstrip(Common):
    def __init__(self, hostname):
        Common.__init__(self, hostname)
    
    def post_open(self):
        self.service = self.device.ledstrip
        return True

    def close(self):
        pass

    def setColor(self, r=0, g=0, b=0, w=0):
        self.service.set('calibrationData', [r, g, b, w])
