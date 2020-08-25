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

    ofp = csv.DictWriter(outputFile, ['requested', 'w_lux', 'rgb_lux', 'rgbw_lux', 'w_cct', 'rgb_cct', 'rgbw_cct', 'lux'])
    ofp.writeheader()
    for requested in VALUES:
        result = {'requested' : requested}
        # Do RGB-only color
        lObj.setColor(r=requested, g=requested, b=requested)
        time.sleep(1)
        sResult = sObj.get()
        result['rgb_lux'] = sResult['lux']
        result['rgb_cct'] = sResult['cct']
        # Do W-only color
        lObj.setColor(w=requested)
        time.sleep(1)
        sResult = sObj.get()
        result['w_lux'] = sResult['lux']
        result['w_cct'] = sResult['cct']
        # Do RGBW color
        lObj.setColor(r=requested, g=requested, b=requested, w=requested)
        time.sleep(1)
        sResult = sObj.get()
        result['rgbw_lux'] = sResult['lux']
        result['rgbw_cct'] = sResult['cct']
        ofp.writerow(result)

    outputFile.flush()
    if args.output:
        outputFile.close()
    del ofp
    sObj.close()
    lObj.close()
    return 0


main()
