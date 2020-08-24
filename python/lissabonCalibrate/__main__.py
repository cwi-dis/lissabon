import sys
import time
import csv
import argparse
from .sensor import Sensor
from .ledstrip import Ledstrip

VALUES = list(map(lambda x : x / 16, range(17)))

def main():
    parser = argparse.ArgumentParser(description="Calibrate lissabonLedstrip using iotsaRGBWSensor")
    parser.add_argument('--ledstrip', '-l', action='store', required=True, metavar='IP', help='Ledstrip hostname')
    parser.add_argument('--sensor', '-s', action='store', required=True, metavar='IP', help='Ledstrip sensor')
    parser.add_argument('--interval', action='store', type=int, metavar='DUR', help='Sensor integration duration (ms, between 40 and 1280)')
    parser.add_argument('--output', '-o', action='store', metavar='OUTPUT', help='CSV output filename')
    args = parser.parse_args()

    sObj = Sensor(args.sensor)
    if not sObj.open(): return -1
    if args.interval:
        sObj.setInterval(args.interval)
    lObj = Ledstrip(args.ledstrip)
    if not lObj.open(): return -1
    if args.output:
        outputFile = open(args.output, 'w')
    else:
        outputFile = sys.stdout

    ofp = csv.DictWriter(outputFile, ['w_req', 'wrgb_req', 'ctrgb_req', 'r', 'g', 'b', 'w', 'lux', 'cct'])
    ofp.writeheader()
    for w in VALUES:
        result = {'w_req' : 0, 'wrgb_req' : w, 'ctrgb_req' : 0}
        lObj.setColor(r=w, g=w, b=w)
        time.sleep(1)
        sResult = sObj.get()
        del sResult['integrationInterval']
        if sResult['w'] >= 0.99:
            print('Warning: --interval too large', file=sys.stderr)
        result.update(sResult)
        ofp.writerow(result)
    for w in VALUES:
        result = {'w_req' : w, 'wrgb_req' : 0, 'ctrgb_req' : 0}
        lObj.setColor(w=w)
        time.sleep(1)
        sResult = sObj.get()
        del sResult['integrationInterval']
        if sResult['w'] >= 0.99:
            print('Warning: --interval too large', file=sys.stderr)
        result.update(sResult)
        ofp.writerow(result)
    for w in VALUES:
        result = {'w_req' : w, 'wrgb_req' : w, 'ctrgb_req' : 0}
        lObj.setColor(r=w, g=w, b=w, w=w)
        time.sleep(1)
        sResult = sObj.get()
        del sResult['integrationInterval']
        if sResult['w'] >= 0.99:
            print('Warning: --interval too large', file=sys.stderr)
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
