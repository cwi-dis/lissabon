import csv
from .sensor import Sensor
from .ledstrip import Ledstrip

VALUES = map(lambda x : x / 16, range(17))

def main():
    sensor = 'colorsensor.local'
    ledstrip = 'striprechts.local'
    output = 'calibration_data.csv'

    sObj = Sensor(sensor)
    lObj = Ledstrip(ledstrip)

    ofp = csv.DictWriter(open(output, 'w'), ['w_req', 'r', 'g', 'b', 'w', 'lux', 'cct'])
    ofp.writeheader()
    for w in VALUES:
        result = {'w_req' : w}
        lObj.setColor(w=w)
        sResult = sObj.get()
        result.update(sResult)
        ofp.writerow(result)
    del ofp



main()
