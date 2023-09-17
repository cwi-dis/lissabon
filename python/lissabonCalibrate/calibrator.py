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
        self.maxlevel = 1.0
        self.args = Namespace()
        self.convertfunc : Callable[[float], Tuple[float, float, float]] = convert_K_to_RGB

    def set_args(self, args : Namespace):
        self.args = args
        if args.cs_cct:
            self.convertfunc = cs_convert_K_to_RGB
        if args.maxlevel == "rgb":
            self.maxlevel = 1.0
        elif args.maxlevel == "max":
            self.maxlevel = 1.0 + args.w_brightness
        elif args.maxlevel == "correct":
            w_r, w_g, w_b = self._get_w_rgb()
            self.maxlevel = 1.0 + min([w_r, w_g, w_b]) * args.w_brightness
            import pdb ; pdb.set_trace()
        else:
            assert f"Unknown maxlevel {args.maxlevel}"
        print(f"xxxjack maxlevel={self.maxlevel}")

    def _get_w_factor(self) -> float:
        """Return the brightness of the W led (relative to the RGB leds combined)"""
        w_factor = 1
        if self.args.w_brightness:
            w_factor = 1 / self.args.w_brightness
        return w_factor

    def _get_w_rgb(self) -> Tuple[float, float, float]:
        """Return the R, G, B relative intensities of the W led given its temperature"""
        r_factor = g_factor = b_factor = 1.0
        if self.args.w_temperature:
            r_factor, g_factor, b_factor = self.convertfunc(self.args.w_temperature)
        return r_factor, g_factor, b_factor

    def _get_ct_rgb(self) -> Tuple[float, float, float]:
        """Return the R, G, B relative intensities of the given temperature"""
        r_factor = g_factor = b_factor = 1.0
        if self.args.temperature:
            r_factor, g_factor, b_factor = self.convertfunc(self.args.temperature)
        return r_factor, g_factor, b_factor

    
    def setstrip_rgb_ct(self, requested_intensity : float):
        if self.args.native:
            self.setstrip_native_ct(requested_intensity, use_rgbw=False)
            return
        
        intensity = requested_intensity * self.maxlevel

        r_factor, g_factor, b_factor = self._get_ct_rgb()
        # Do RGB-only color
        if self.verbose: print(f'Set RGB lux level={intensity} CT={self.args.temperature}', file=sys.stderr)
        this_r = (intensity*r_factor) ** self.args.gamma
        this_g = (intensity*g_factor) ** self.args.gamma
        this_b = (intensity*b_factor) ** self.args.gamma
        if this_r > 1:
            if self.verbose: print(f"Set RGB: R clipped from {this_r}")
            this_r = 1
        if this_g > 1:
            if self.verbose: print(f"Set RGB: G clipped from {this_g}")
            this_g = 1
        if this_b > 1:
            if self.verbose: print(f"Set RGB: B clipped from {this_b}")
            this_b = 1
        self.ledstrip.setColor(r=this_r, g=this_g, b=this_b)

    def setstrip_w(self, requested_intensity : float):
        intensity = requested_intensity * self.maxlevel
        w_factor = self._get_w_factor()
        if self.verbose: print(f'Set W lux level={intensity} CT={self.args.w_temperature}', file=sys.stderr)
        this_w = (intensity*w_factor) ** self.args.gamma
        if this_w > 1:
            if self.verbose: print(f"Set W: W clipped from {this_w}")
            this_w = 1
        self.ledstrip.setColor(w=this_w)
    
    def setstrip_rgbw_ct(self, requested_intensity : float):
        if self.args.native:
            self.setstrip_native_ct(requested_intensity, use_rgbw=True)
            return
        # Scale intensity to cater for the W led
        intensity = requested_intensity * self.maxlevel
        # Determine RGB for wanted temperature (default: all equal, probably 6500)
        r_factor, g_factor, b_factor = self._get_ct_rgb()
        # Determine levels for RGB-only
        intensity_multiplier = 1 # xxxjack alternative: 1+w_brightness, or something in between
        r_wanted_rgb = r_factor * intensity * intensity_multiplier
        g_wanted_rgb = g_factor * intensity * intensity_multiplier
        b_wanted_rgb = b_factor * intensity * intensity_multiplier
        # Determine W relative brightness (default: same as RGB)
        w_factor = self._get_w_factor()
        # Determine RGB for W temperature (default: same as RGB leds)
        w_led_rgb_r, w_led_rgb_g, w_led_rgb_b = self._get_w_rgb()
        if self.verbose: print(f'Set RGBW lux level={intensity} CT={self.args.temperature}', file=sys.stderr)
        # Now determine how much intensity we can remove from RGB leds and transfer to W led
        print(f"\txxxjack RGBW-RGB {r_factor} {g_factor} {b_factor} W-LED {w_led_rgb_r} {w_led_rgb_g} {w_led_rgb_b}")
        max_r_remove = r_wanted_rgb / w_led_rgb_r
        max_g_remove = g_wanted_rgb / w_led_rgb_g
        max_b_remove = b_wanted_rgb / w_led_rgb_b
        transfer_to_w = min(max_r_remove, max_g_remove, max_b_remove)
        if transfer_to_w > (1/w_factor):
            print(f"Set RGBW: clip W-transfer from {transfer_to_w} to {1/w_factor}")
            transfer_to_w = 1/w_factor
        assert transfer_to_w >= 0
        r_wanted_rgbw = r_wanted_rgb - (transfer_to_w * w_led_rgb_r)
        g_wanted_rgbw = g_wanted_rgb - (transfer_to_w * w_led_rgb_g)
        b_wanted_rgbw = b_wanted_rgb - (transfer_to_w * w_led_rgb_b)
        w_wanted_rgbw = transfer_to_w * w_factor
        assert r_wanted_rgbw >= 0 
        assert g_wanted_rgbw >= 0 
        assert b_wanted_rgbw >= 0 
        assert w_wanted_rgbw >= 0
        this_r = r_wanted_rgbw ** self.args.gamma
        this_g = g_wanted_rgbw ** self.args.gamma
        this_b = b_wanted_rgbw ** self.args.gamma
        this_w = w_wanted_rgbw ** self.args.gamma
        if this_r > 1:
            if self.verbose: print(f"Set RGBW: R clipped from {this_r}")
            this_r = 1
        if this_g > 1:
            if self.verbose: print(f"Set RGBW: G clipped from {this_g}")
            this_g = 1
        if this_b > 1:
            if self.verbose: print(f"Set RGBW: B clipped from {this_b}")
            this_b = 1
        if this_w > 1:
            if self.verbose: print(f"Set RGBW: W clipped from {this_w}")
            this_w = 1
        assert this_r >= 0 and this_r <= 1
        assert this_g >= 0 and this_g <= 1
        assert this_b >= 0 and this_b <= 1
        assert this_w >= 0 and this_w <= 1
        print(f"\txxxjack RGBW {this_r} {this_g} {this_b} {this_w}")
        self.ledstrip.setColor(r=this_r, g=this_g, b=this_b, w=this_w)

    def setstrip_native_ct(self, intensity : float, use_rgbw=True):
        assert self.args.w_temperature
        if self.verbose: print(f'Set CT {self.args.temperature} {intensity}', 'rgbw' if use_rgbw else 'rgb')
        self.ledstrip.setCT(
            intensity, 
            self.args.temperature, 
            useRGBW=use_rgbw, 
            whiteTemperature=self.args.w_temperature,
            whiteBrightness=self.args.w_brightness
        )

    def run_leds(self):
        keys = ['label', 'req_r', 'req_g', 'req_b', 'req_w', 'rgb_r', 'rgb_g', 'rgb_b', 'lux']
        wtd_values = [
            ('white', 0, 0, 0, 0.5),
            ('red', 0.5, 0, 0, 0),
            ('green', 0, 0.5, 0, 0),
            ('blue', 0, 0, 0.5, 0),
            ('rgb', 0.5, 0.5, 0.5, 0),
        ]
        for i in range(11):
            wtd_values.append(
                ('triangle', 1-(0.1*i), 0.1*i, 0, 0) # Red to green
            )
        for i in range(11):
            wtd_values.append(
                ('triangle', 0, 1-(0.1*i), 0.1*i, 0)
            )
        for i in range(11):
            wtd_values.append(
                ('triangle', 0.1*i, 0, 1-(0.1*i), 0)
            )
        values = []
        for label, r, g, b, w in wtd_values:
            result = dict(label=label, req_r=r, req_g=g, req_b=b, req_w=w)
            self.ledstrip.setColor(r, g, b, w)
            sResult = self.sensor.get()
            result['lux'] = sResult['lux']
            result['rgb_r'] = sResult['r']
            result['rgb_g'] = sResult['g']
            result['rgb_b'] = sResult['b']
            values.append(result)
        parameters = dict(
            measurement='leds',
            )
        return keys, values, parameters
    
    def run_lux(self):
        args = self.args
        nsteps = args.steps

        intensity_levels_wanted : List[float] = list(map(lambda x : x / nsteps, range(nsteps+1)))
        keys = ['requested', 'w_white', 'rgb_white', 'rgbw_white', 'rgb_r', 'rgb_g', 'rgb_b', 'w_lux', 'rgb_lux', 'rgbw_lux', 'w_cct', 'rgb_cct', 'rgbw_cct']
        results = []

        for requested in intensity_levels_wanted:
            result = {'requested' : requested}
            #
            # Measure RGB-only intensity with the requested RGB temperature
            #
            self.setstrip_rgb_ct(requested)

            time.sleep(1)
            sResult = self.sensor.get()
            result['rgb_white'] = sResult['w']
            result['rgb_lux'] = sResult['lux']
            result['rgb_cct'] = sResult['cct']
            result['rgb_r'] = sResult['r']
            result['rgb_g'] = sResult['g']
            result['rgb_b'] = sResult['b']
            #
            # Measure W-only intensity
            #
            self.setstrip_w(requested)
            time.sleep(1)
            sResult = self.sensor.get()
            result['w_white'] = sResult['w']
            result['w_lux'] = sResult['lux']
            result['w_cct'] = sResult['cct']
            #
            # Measure RGBW intensity given the W led relative brightness and RGB CT
            #
            self.setstrip_rgbw_ct(requested)
            time.sleep(1)
            sResult = self.sensor.get()
            result['rgbw_white'] = sResult['w']
            result['rgbw_lux'] = sResult['lux']
            result['rgbw_cct'] = sResult['cct']
            results.append(result)        
        parameters = dict(
            measurement='lux',
            interval=args.interval,
            gamma=args.gamma,
            temperature=args.temperature,
            w_brightness=args.w_brightness or 0,
            maxlevel=self.maxlevel,
            method='native' if args.native else 'cs_cct' if args.cs_cct else 'python'
            )
        if args.w_temperature and args.w_temperature != args.temperature:
            parameters['w_temperature'] = args.w_temperature
        return keys, results, parameters
            
    def run_cct(self):
        args = self.args
        nsteps = args.steps

        MIN_CCT = 2000
        MAX_CCT = 7000
        do_rgbw =  'w_temperature' in args and args.w_temperature
        w_factor = 0
        if do_rgbw:
            w_factor = 1 / args.w_brightness
            w_led_rgb_r, w_led_rgb_g, w_led_rgb_b = self.convertfunc(args.w_temperature)
            if self.verbose:
                print(f'W LED temperature {args.w_temperature}, brightness {args.w_brightness}, r {w_led_rgb_r}, g {w_led_rgb_g}, b {w_led_rgb_b}')
        ct_values_wanted : List[float] = []
        for i in range(nsteps+1):
            ct_values_wanted.append(MIN_CCT + (i/nsteps*(MAX_CCT-MIN_CCT)))
        if do_rgbw and not args.w_temperature in ct_values_wanted:
            ct_values_wanted.append(args.w_temperature)
            ct_values_wanted.sort()

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

        results : List[dict] = []

        for requested in ct_values_wanted:
            result = {'requested' : requested}
            r_wanted, g_wanted, b_wanted = self.convertfunc(requested)
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
                this_r = (r_wanted*level) ** args.gamma
                this_g = (g_wanted*level) ** args.gamma
                this_b = (b_wanted*level) ** args.gamma
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
                    this_r = (r_wanted_rgbw*level) ** args.gamma
                    this_g = (g_wanted_rgbw*level) ** args.gamma
                    this_b = (b_wanted_rgbw*level) ** args.gamma
                    this_w = (w_wanted_rgbw*level) ** args.gamma
                    self.ledstrip.setColor(r=this_r, g=this_g, b=this_b, w=this_w)
                    time.sleep(1)
                    sResult = self.sensor.get()
                    result[f'rgbw_lux_{percent}'] = sResult['lux']
                    result[f'rgbw_cct_{percent}'] = sResult['cct']
                    result[f'rgbw_r_{percent}'] = sResult['r']
                    result[f'rgbw_g_{percent}'] = sResult['g']
                    result[f'rgbw_b_{percent}'] = sResult['b']
                    result[f'rgbw_w_{percent}'] = sResult['w']

            results.append(result)        
        parameters = dict(
            measurement='cct',
            interval=args.interval,
            gamma=args.gamma,
            w_temperature=args.w_temperature,
            w_factor=w_factor,
            cs_cct=args.cs_cct
            )
        return keys, results, parameters