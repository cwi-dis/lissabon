import sys
import time
import csv
import argparse
from .sensor import Sensor
from .ledstrip import Ledstrip
from .colorconvert import convert_K_to_RGB
from .calibrator import Calibrator
from .plot import plot_lines, plot_colors

def write_csv(fp, keys, values, parameters):
    ofp = csv.DictWriter(fp, keys + ['parameter', 'value'], quoting=csv.QUOTE_NONNUMERIC)
    ofp.writeheader()
    for v in values:
        ofp.writerow(v)
    # Finally dump parameters and values
    for k, v in parameters.items():
        ofp.writerow(dict(parameter=k, value=v))
    fp.flush()

def read_csv(fp):
    reader = csv.DictReader(fp, quoting=csv.QUOTE_NONNUMERIC)
    keys = None
    values = []
    parameters = {}
    for row in reader:
        # Initialize keys, if not done yet
        if keys == None:
            keys = list(row.keys())
            keys.remove('parameter')
            keys.remove('value')
        # Check whether this is a parameter
        if row['parameter']:
            parameters[row['parameter']] = row['value']
            continue
        # Remove empty parameter/value fields
        del row['parameter']
        del row['value']
        values.append(row)
    return keys, values, parameters
        
def main():
    parser = argparse.ArgumentParser(description="Calibrate lissabonLedstrip using iotsaRGBWSensor")
    parser.add_argument('--measurement', '-m', action='store', choices=['rgbw_lux', 'rgb_cct'], help='Type of mesurement to do')
    parser.add_argument('--ledstrip', '-l', action='store', metavar='IP', help='Ledstrip hostname')
    parser.add_argument('--sensor', '-s', action='store', metavar='IP', help='Ledstrip sensor')
    parser.add_argument('--interval', action='store', type=int, metavar='DUR', help='Sensor integration duration (ms, between 40 and 1280)')
    parser.add_argument('--steps', action='store', type=int, default=16, metavar='N', help='Use N steps for calibration (default 16)')
    parser.add_argument('--w_gamma', action='store', default=1, type=float, metavar='GAMMA', help='Gamma value for W channel (default 1.0)')
    parser.add_argument('--w_brightness', action='store', type=float, metavar='W', help='Treat W channel as having this brightness relative to RGB  (default 1.0)')
    parser.add_argument('--rgb_gamma', action='store', default=1, type=float, metavar='GAMMA', help='Gamma value for W channel (default 1.0)')
    parser.add_argument('--g_hack', action='store', type=float, metavar='FACTOR', help='G-channel multiplication factor (default 1.0)')
    parser.add_argument('--b_hack', action='store', type=float, metavar='FACTOR', help='B-channel multiplication factor (default 1.0)')
    parser.add_argument('--rgb_temperature', action='store', type=float, metavar='KELVIN', help='Color temperature for RGB (default: no correction)')
    parser.add_argument('--cs_cct', action='store_true', help='Use colourscience for CCT to RGB conversion')
    
    parser.add_argument('--input', action='store', metavar='INPUT', help="CSV input filename, skips measurement but reads previous data from previous run")
    parser.add_argument('--csv', '-o', action='store', metavar='OUTPUT', help='CSV output filename')
    parser.add_argument('--plot', action='store_true', help='Show output as a plot')
    
    args = parser.parse_args()

    if not args.input and not (args.measurement and args.sensor and args.ledstrip):
            parser.print_usage(sys.stderr)
            print('Either --input or all of --measurement, --ledstrip and --sensor must be specified', file=sys.stderr)
            return -1
    if not args.csv and not args.plot:
            parser.print_usage(sys.stderr)
            print('Either --csv or --plot must be specified', file=sys.stderr)
            return -1
    sObj = None
    lObj = None
    if args.input:
        keys, values, parameters = read_csv(open(args.input))
    else:
        sObj = Sensor(args.sensor)
        if not sObj.open(): return -1
        if args.interval:
            sObj.setInterval(args.interval)

        lObj = Ledstrip(args.ledstrip)
        if not lObj.open(): return -1
    

        calibrator = Calibrator(sObj, lObj)
    
        if args.measurement == 'rgbw_lux':
            keys, values, parameters = calibrator.run_rgbw_lux(args.steps, args)
        elif args.measurement == 'rgb_cct':
            keys, values, parameters = calibrator.run_rgb_cct(args.steps, args)
        else:
            assert False, f'Unknown measurement type {args.measurement}'
    
    if args.csv:
        outputFile = open(args.csv, 'w')
        write_csv(outputFile, keys, values, parameters)
        outputFile.close()
        
    if args.plot:
        if parameters['measurement'] == 'rgbw_lux':
            plot_lines(values, parameters, 'requested', ['w_lux', 'rgb_lux', 'rgbw_lux'])
        elif parameters['measurement'] == 'rgb_cct':
            plot_colors(values, parameters, ['50'])
            #plot_colors(values, parameters, ['100'])
        else:
            assert False, f'Unknown measurement type {parameters["measurement"]}'

    if sObj: sObj.close()
    if lObj: lObj.close()
    return 0


main()
