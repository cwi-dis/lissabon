import sys
import time
from argparse import Namespace
from typing import List, Callable, Tuple
from .sensor import Sensor
from .ledstrip import Ledstrip
from .colorconvert import convert_K_to_RGB
from .colourscience_cc import cs_convert_K_to_RGB


class Calibrator:
    def __init__(self, sensor : Sensor, ledstrip : Ledstrip):
        self.sensor = sensor
        self.ledstrip = ledstrip
        self.verbose = True
        
    def run_lux(self, nsteps : int, args : Namespace):
        VALUES : List[float] = list(map(lambda x : x / nsteps, range(nsteps+1)))
        keys = ['requested', 'w_white', 'rgb_white', 'rgbw_white', 'rgb_r', 'rgb_g', 'rgb_b', 'w_lux', 'rgb_lux', 'rgbw_lux', 'w_cct', 'rgb_cct', 'rgbw_cct']
        values = []

        r_factor = g_factor = b_factor = w_factor = 1
        if args.w_brightness:
            w_factor = 1 / args.w_brightness
        if args.rgb_temperature:
            r_factor, g_factor, b_factor = convert_K_to_RGB(args.rgb_temperature)
        if args.g_hack:
            g_factor = g_factor * args.g_hack
        if args.b_hack:
            b_factor = b_factor * args.b_hack
        for requested in VALUES:
            result = {'requested' : requested}
            w_wanted = requested
            rgb_wanted = requested
            # Do RGB-only color
            if self.verbose: print(f'Measure RGB lux level={requested}', file=sys.stderr)
            this_r = (rgb_wanted*r_factor) ** args.rgb_gamma
            this_g = (rgb_wanted*g_factor) ** args.rgb_gamma
            this_b = (rgb_wanted*b_factor) ** args.rgb_gamma
            self.ledstrip.setColor(r=this_r, g=this_g, b=this_b)
            time.sleep(1)
            sResult = self.sensor.get()
            result['rgb_white'] = sResult['w']
            result['rgb_lux'] = sResult['lux']
            result['rgb_cct'] = sResult['cct']
            result['rgb_r'] = sResult['r']
            result['rgb_g'] = sResult['g']
            result['rgb_b'] = sResult['b']
            # Do W-only color
            if self.verbose: print(f'Measure W lux level={requested}', file=sys.stderr)
            this_w = (w_wanted*w_factor) ** args.w_gamma
            self.ledstrip.setColor(w=this_w)
            time.sleep(1)
            sResult = self.sensor.get()
            result['w_white'] = sResult['w']
            result['w_lux'] = sResult['lux']
            result['w_cct'] = sResult['cct']
            # Do RGBW color
            if self.verbose: print(f'Measure RGBW lux level={requested}', file=sys.stderr)
            this_r = (rgb_wanted*r_factor*0.5) ** args.rgb_gamma
            this_g = (rgb_wanted*g_factor*0.5) ** args.rgb_gamma
            this_b = (rgb_wanted*b_factor*0.5) ** args.rgb_gamma
            this_w = (w_wanted*w_factor*0.5) ** args.w_gamma
            self.ledstrip.setColor(r=this_r, g=this_g, b=this_b, w=this_w)
            time.sleep(1)
            sResult = self.sensor.get()
            result['rgbw_white'] = sResult['w']
            result['rgbw_lux'] = sResult['lux']
            result['rgbw_cct'] = sResult['cct']
            values.append(result)        
        parameters = dict(
            measurement='lux',
            interval=args.interval,
            w_gamma=args.w_gamma,
            rgb_gamma=args.rgb_gamma,
            rgb_temperature=args.rgb_temperature,
            w_temperature=args.w_temperature,
            r_factor=r_factor,
            g_factor=g_factor,
            b_factor=b_factor,
            w_factor=w_factor,
            )
        return keys, values, parameters
            
    def run_cct(self, nsteps : int, args : Namespace):
        convertfunc : Callable[[float], Tuple[float, float, float]] = convert_K_to_RGB
        if args.cs_cct:
            convertfunc = cs_convert_K_to_RGB
        MIN_CCT = 2000
        MAX_CCT = 7000
        do_rgbw =  'w_temperature' in args and args.w_temperature
        w_factor = 0
        if do_rgbw:
            w_factor = 1 / args.w_brightness
            w_led_rgb_r, w_led_rgb_g, w_led_rgb_b = convertfunc(args.w_temperature)
            if self.verbose:
                print(f'W LED temperature {args.w_temperature}, brightness {args.w_brightness}, r {w_led_rgb_r}, g {w_led_rgb_g}, b {w_led_rgb_b}')
        VALUES : List[float] = []
        for i in range(nsteps+1):
            VALUES.append(MIN_CCT + (i/nsteps*(MAX_CCT-MIN_CCT)))
        if do_rgbw and not args.w_temperature in VALUES:
            VALUES.append(args.w_temperature)
            VALUES.sort()

        keys = ['requested']
        for percent in [10, 20, 50, 100]:
            keys.append(f'rgb_lux_{percent}')
            keys.append(f'rgb_cct_{percent}')
            keys.append(f'rgb_r_{percent}')
            keys.append(f'rgb_g_{percent}')
            keys.append(f'rgb_b_{percent}')
            keys.append(f'rgb_w_{percent}')
            if do_rgbw:
                keys.append(f'rgbw_lux_{percent}')
                keys.append(f'rgbw_cct_{percent}')
                keys.append(f'rgbw_r_{percent}')
                keys.append(f'rgbw_g_{percent}')
                keys.append(f'rgbw_b_{percent}')
                keys.append(f'rgbw_w_{percent}')

        values : List[dict] = []

        for requested in VALUES:
            result = {'requested' : requested}
            r_wanted, g_wanted, b_wanted = convertfunc(requested)
            if self.verbose:
                print(f'RGB r={r_wanted}, g={g_wanted}, b={b_wanted}')
            if do_rgbw:
                # Determine how much we can transfer from the RGB channels to the W channel
                max_r_factor = r_wanted / w_led_rgb_r
                max_g_factor = g_wanted / w_led_rgb_g
                max_b_factor = b_wanted / w_led_rgb_b
                transfer_to_w = min(max_r_factor, max_g_factor, max_b_factor)
                assert transfer_to_w >= 0
                assert transfer_to_w <= 1
                r_wanted_rgbw = r_wanted - (transfer_to_w * w_led_rgb_r)
                g_wanted_rgbw = g_wanted - (transfer_to_w * w_led_rgb_g)
                b_wanted_rgbw = b_wanted - (transfer_to_w * w_led_rgb_b)
                w_wanted_rgbw = transfer_to_w * w_factor
                if self.verbose:
                    print(f'RGBW r={r_wanted_rgbw}, g={g_wanted_rgbw}, b={b_wanted_rgbw}, w={w_wanted_rgbw}')
                # import pdb ; pdb.set_trace()
                
            for percent in [10, 20, 50, 100]:
                level = percent / 100
                if self.verbose: print(f'Measure RGB CCT level={level} cct={requested}', file=sys.stderr)
                # Do RGB-only color
                this_r = (r_wanted*level) ** args.rgb_gamma
                this_g = (g_wanted*level) ** args.rgb_gamma
                this_b = (b_wanted*level) ** args.rgb_gamma
                self.ledstrip.setColor(r=this_r, g=this_g, b=this_b)
                time.sleep(1)
                sResult = self.sensor.get()
                result[f'rgb_lux_{percent}'] = sResult['lux']
                result[f'rgb_cct_{percent}'] = sResult['cct']
                result[f'rgb_r_{percent}'] = sResult['r']
                result[f'rgb_g_{percent}'] = sResult['g']
                result[f'rgb_b_{percent}'] = sResult['b']
                result[f'rgb_w_{percent}'] = sResult['w']
                if do_rgbw:
                    if self.verbose: print(f'Measure RGBW CCT level={level} cct={requested}', file=sys.stderr)
                    this_r = (r_wanted_rgbw*level) ** args.rgb_gamma
                    this_g = (g_wanted_rgbw*level) ** args.rgb_gamma
                    this_b = (b_wanted_rgbw*level) ** args.rgb_gamma
                    this_w = (w_wanted_rgbw*level) ** args.w_gamma
                    self.ledstrip.setColor(r=this_r, g=this_g, b=this_b, w=this_w)
                    time.sleep(1)
                    sResult = self.sensor.get()
                    result[f'rgbw_lux_{percent}'] = sResult['lux']
                    result[f'rgbw_cct_{percent}'] = sResult['cct']
                    result[f'rgbw_r_{percent}'] = sResult['r']
                    result[f'rgbw_g_{percent}'] = sResult['g']
                    result[f'rgbw_b_{percent}'] = sResult['b']
                    result[f'rgbw_w_{percent}'] = sResult['w']

            values.append(result)        
        parameters = dict(
            measurement='cct',
            interval=args.interval,
            w_gamma=args.w_gamma,
            rgb_gamma=args.rgb_gamma,
            rgb_temperature=args.rgb_temperature,
            w_temperature=args.w_temperature,
            w_factor=w_factor,
            cs_cct=args.cs_cct
            )
        return keys, values, parameters