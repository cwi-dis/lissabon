from .sensor import Sensor
import sys
import time
from .ledstrip import Ledstrip
from .colorconvert import convert_K_to_RGB


class Calibrator:
    def __init__(self, sensor, ledstrip):
        self.sensor = sensor
        self.ledstrip = ledstrip
        
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
            this_w = (w_wanted*w_factor) ** args.w_gamma
            self.ledstrip.setColor(w=this_w)
            time.sleep(1)
            sResult = self.sensor.get()
            result['w_white'] = sResult['w']
            result['w_lux'] = sResult['lux']
            result['w_cct'] = sResult['cct']
            # Do RGBW color
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