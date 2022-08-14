from typing import Tuple
import colour
import numpy as np

def cs_convert_K_to_RGB(colour_temperature : float) -> Tuple[float, float, float]:
    xy = colour.CCT_to_xy(colour_temperature)
    xyY = np.array([xy[0], xy[1], 1])
    XYZ = colour.xyY_to_XYZ(xyY)
    sRGB = colour.RGB_COLOURSPACES['sRGB']
    RGB = colour.XYZ_to_RGB(XYZ, sRGB.whitepoint, sRGB.whitepoint, sRGB.XYZ_to_RGB_matrix)
    return RGB[0], RGB[1], RGB[2]


