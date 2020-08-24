import sys
import csv
import argparse
from .sensor import Sensor
from .ledstrip import Ledstrip

VALUES = map(lambda x : x / 16, range(17))

def main():
    parser = argparse.ArgumentParser(description="Calibrate lissabonLedstrip using iotsaRGBWSensor")
    parser.add_argument('--ledstrip', '-l', action='store', required=True, metavar='IP', help='Ledstrip hostname')
    parser.add_argument('--sensor', '-s', action='store', required=True, metavar='IP', help='Ledstrip sensor')
    parser.add_argument('--output', '-o', action='store', metavar='OUTPUT', help='CSV output filename')
    args = parser.parse_args()

    sObj = Sensor(args.sensor)
    if not sObj.open(): return -1
    lObj = Ledstrip(args.ledstrip)
    if not lObj.open(): return -1
    if args.output:
        outputFile = open(args.output, 'w')
    else:
        outputFile = sys.stdout

    ofp = csv.DictWriter(outputFile, ['w_req', 'r', 'g', 'b', 'w', 'lux', 'cct'])
    ofp.writeheader()
    for w in VALUES:
        result = {'w_req' : w}
        lObj.setColor(w=w)
        sResult = sObj.get()
        del sResult['integrationInterval']
        result.update(sResult)
        ofp.writerow(result)
    outputFile.flush()
    if args.output:
        outputFile.close()
    del ofp
    sObj.close()
    lObj.close()
    return 0


main()
