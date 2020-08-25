import sys
import time
import csv
import argparse
from .sensor import Sensor
from .ledstrip import Ledstrip
from .colorconvert import convert_K_to_RGB

VALUES = list(map(lambda x : x / 16, range(17)))

def main():
    parser = argparse.ArgumentParser(description="Calibrate lissabonLedstrip using iotsaRGBWSensor")
    parser.add_argument('--ledstrip', '-l', action='store', required=True, metavar='IP', help='Ledstrip hostname')
    parser.add_argument('--sensor', '-s', action='store', required=True, metavar='IP', help='Ledstrip sensor')
    parser.add_argument('--interval', action='store', type=int, metavar='DUR', help='Sensor integration duration (ms, between 40 and 1280)')
    parser.add_argument('--w_gamma', action='store', type=float, metavar='GAMMA', help='Gamma value for W channel (default 1.0)')
    parser.add_argument('--w_brightness', action='store', type=float, metavar='W', help='Treat W channel as having this brightness relative to RGB  (default 1.0)')
    parser.add_argument('--rgb_gamma', action='store', type=float, metavar='GAMMA', help='Gamma value for W channel (default 1.0)')
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
    
    r_factor = g_factor = b_factor = w_factor = 1
    if args.w_brightness:
        w_factor = 1 / args.w_brightness
    if args.rgb_temperature:
        r_factor, g_factor, b_factor = convert_K_to_RGB(args.rgb_temperature)
    if args.g_hack:
        g_factor = g_factor * args.g_hack
    if args.b_hack:
        b_factor = b_factor * args.b_hack
    ofp = csv.DictWriter(outputFile, ['requested', 'w_white', 'rgb_white', 'rgbw_white', 'rgb_r', 'rgb_g', 'rgb_b', 'w_lux', 'rgb_lux', 'rgbw_lux', 'w_cct', 'rgb_cct', 'rgbw_cct', 'parameter', 'value'])
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
        lObj.setColor(r=rgb_wanted*r_factor, g=rgb_wanted*g_factor, b=rgb_wanted*b_factor)
        time.sleep(1)
        sResult = sObj.get()
        result['rgb_white'] = sResult['w']
        result['rgb_lux'] = sResult['lux']
        result['rgb_cct'] = sResult['cct']
        result['rgb_r'] = sResult['r']
        result['rgb_g'] = sResult['g']
        result['rgb_b'] = sResult['b']
        # Do W-only color
        lObj.setColor(w=w_wanted*w_factor)
        time.sleep(1)
        sResult = sObj.get()
        result['w_white'] = sResult['w']
        result['w_lux'] = sResult['lux']
        result['w_cct'] = sResult['cct']
        # Do RGBW color
        lObj.setColor(r=rgb_wanted*r_factor*0.5, g=rgb_wanted*g_factor*0.5, b=rgb_wanted*b_factor*0.5, w=w_wanted*w_factor*0.5)
        time.sleep(1)
        sResult = sObj.get()
        result['rgbw_white'] = sResult['w']
        result['rgbw_lux'] = sResult['lux']
        result['rgbw_cct'] = sResult['cct']
        ofp.writerow(result)
    # Finally dump parameters and values
    ofp.writerow(dict(parameter='interval', value=args.interval))
    ofp.writerow(dict(parameter='w_gamma', value=args.w_gamma))
    ofp.writerow(dict(parameter='rgb_gamma', value=args.rgb_gamma))
    ofp.writerow(dict(parameter='rgb_temperature', value=args.rgb_temperature))
    ofp.writerow(dict(parameter='r_factor', value=r_factor))
    ofp.writerow(dict(parameter='g_factor', value=g_factor))
    ofp.writerow(dict(parameter='b_factor', value=b_factor))
    ofp.writerow(dict(parameter='w_factor', value=w_factor))
    outputFile.flush()
    if args.output:
        outputFile.close()
    del ofp
    sObj.close()
    lObj.close()
    return 0


main()
