from .common import Common

class Ledstrip(Common):
    def __init__(self, hostname):
        Common.__init__(self, hostname)
    
    def post_open(self):
        self.service = self.device.ledstrip
        # Disable automatic sleep for a few minutes
        self.device.battery.set('postponeSleep', 120000)
        return True

    def close(self):
        self.device.battery.set('postponeSleep', 0)
        pass

    def setColor(self, r=0, g=0, b=0, w=0):
        self.service.set('calibrationData', [r, g, b, w])
