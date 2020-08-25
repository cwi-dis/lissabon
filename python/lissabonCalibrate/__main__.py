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
    parser.add_argument('--w_gamma', action='store', type=float, metavar='GAMMA', help='Gamma value for W channel (default 1.0)')
    parser.add_argument('--rgb_gamma', action='store', type=float, metavar='GAMMA', help='Gamma value for W channel (default 1.0)')
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

    ofp = csv.DictWriter(outputFile, ['requested', 'w_lux', 'rgb_lux', 'rgbw_lux', 'w_cct', 'rgb_cct', 'rgbw_cct', 'parameter', 'value'])
    ofp.writeheader()
    for requested in VALUES:
        result = {'requested' : requested}
        w_wanted = requested
        rgb_wanted = requested
        if args.w_gamma:
            w_wanted = w_wanted ** args.w_gamma
        if args.rgb_gamma:
            rgb_wanted = rgb_wanted ** args.rgb_gamma
        # Do RGB-only color
        lObj.setColor(r=rgb_wanted, g=rgb_wanted, b=rgb_wanted)
        time.sleep(1)
        sResult = sObj.get()
        result['rgb_lux'] = sResult['lux']
        result['rgb_cct'] = sResult['cct']
        # Do W-only color
        lObj.setColor(w=w_wanted)
        time.sleep(1)
        sResult = sObj.get()
        result['w_lux'] = sResult['lux']
        result['w_cct'] = sResult['cct']
        # Do RGBW color
        lObj.setColor(r=rgb_wanted, g=rgb_wanted, b=rgb_wanted, w=w_wanted)
        time.sleep(1)
        sResult = sObj.get()
        result['rgbw_lux'] = sResult['lux']
        result['rgbw_cct'] = sResult['cct']
        ofp.writerow(result)
    # Finally dump parameters and values
    ofp.writerow(dict(parameter='interval', value=args.interval))
    ofp.writerow(dict(parameter='w_gamma', value=args.w_gamma))
    ofp.writerow(dict(parameter='rgb_gamma', value=args.rgb_gamma))
    outputFile.flush()
    if args.output:
        outputFile.close()
    del ofp
    sObj.close()
    lObj.close()
    return 0


main()
