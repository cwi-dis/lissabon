import sys
import time
import csv
import argparse
from .sensor import Sensor
from .ledstrip import Ledstrip
from .colorconvert import convert_K_to_RGB
from .calibrator import Calibrator


def write_csv(fp, keys, values, parameters):
    ofp = csv.DictWriter(fp, keys + ['parameter', 'value'])
    ofp.writeheader()
    for v in values:
        ofp.writerow(v)
    # Finally dump parameters and values
    for k, v in parameters.items():
        ofp.writerow(dict(parameter=k, value=v))
    fp.flush()

def main():
    parser = argparse.ArgumentParser(description="Calibrate lissabonLedstrip using iotsaRGBWSensor")
    parser.add_argument('--ledstrip', '-l', action='store', required=True, metavar='IP', help='Ledstrip hostname')
    parser.add_argument('--sensor', '-s', action='store', required=True, metavar='IP', help='Ledstrip sensor')
    parser.add_argument('--interval', action='store', type=int, metavar='DUR', help='Sensor integration duration (ms, between 40 and 1280)')
    parser.add_argument('--steps', action='store', type=int, default=16, metavar='N', help='Use N steps for calibration (default 16)')
    parser.add_argument('--w_gamma', action='store', default=1, type=float, metavar='GAMMA', help='Gamma value for W channel (default 1.0)')
    parser.add_argument('--w_brightness', action='store', type=float, metavar='W', help='Treat W channel as having this brightness relative to RGB  (default 1.0)')
    parser.add_argument('--rgb_gamma', action='store', default=1, type=float, metavar='GAMMA', help='Gamma value for W channel (default 1.0)')
    parser.add_argument('--g_hack', action='store', type=float, metavar='FACTOR', help='G-channel multiplication factor (default 1.0)')
    parser.add_argument('--b_hack', action='store', type=float, metavar='FACTOR', help='B-channel multiplication factor (default 1.0)')
    parser.add_argument('--rgb_temperature', action='store', type=float, metavar='KELVIN', help='Color temperature for RGB (default: no correction)')
    
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

    calibrator = Calibrator(sObj, lObj)
    
    keys, values, parameters = calibrator.run_rgbw_lux(args.steps, args)
    
    write_csv(outputFile, keys, values, parameters)
    
    if args.output:
        outputFile.close()
    sObj.close()
    lObj.close()
    return 0


main()
