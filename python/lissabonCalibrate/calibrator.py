from .sensor import Sensor
import sys
import time
from .ledstrip import Ledstrip
from .colorconvert import convert_K_to_RGB
from .colourscience_cc import cs_convert_K_to_RGB


class Calibrator:
    def __init__(self, sensor, ledstrip):
        self.sensor = sensor
        self.ledstrip = ledstrip
        self.verbose = True
        
    def run_rgbw_lux(self, nsteps, args):
        VALUES = list(map(lambda x : x / nsteps, range(nsteps+1)))
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
            measurement='rgbw_lux',
            interval=args.interval,
            w_gamma=args.w_gamma,
            rgb_gamma=args.rgb_gamma,
            rgb_temperature=args.rgb_temperature,
            r_factor=r_factor,
            g_factor=g_factor,
            b_factor=b_factor,
            w_factor=w_factor,
            )
        return keys, values, parameters
            
    def run_rgb_cct(self, nsteps, args):
        convertfunc = convert_K_to_RGB
        if args.cs_cct:
            convertfunc = cs_convert_K_to_RGB
        MIN_CCT = 2000
        MAX_CCT = 7000
        VALUES = []
        for i in range(nsteps+1):
            VALUES.append(MIN_CCT + (i/nsteps*(MAX_CCT-MIN_CCT)))

        keys = ['requested']
        for percent in [10, 20, 50, 100]:
            keys.append(f'rgb_lux_{percent}')
            keys.append(f'rgb_cct_{percent}')
            keys.append(f'rgb_r_{percent}')
            keys.append(f'rgb_g_{percent}')
            keys.append(f'rgb_b_{percent}')
            keys.append(f'rgb_w_{percent}')
        values = []

        for requested in VALUES:
            result = {'requested' : requested}
            r_wanted, g_wanted, b_wanted = convertfunc(requested)
            #for percent in [10, 20, 50, 100]:
            for percent in [50]:
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

            values.append(result)        
        parameters = dict(
            measurement='rgb_cct',
            interval=args.interval,
            rgb_gamma=args.rgb_gamma,
            cs_cct=args.cs_cct
            )
        return keys, values, parameters