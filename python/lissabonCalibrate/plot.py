import numpy as np
import matplotlib.pyplot
from typing import List, Optional
import colour
import colour.plotting

def plot_lines(values : List[dict], parameters : dict, xlabel : str, ylabels : List[str], y2labels : Optional[List[str]]=None, variableName : str='LUX', filename : Optional[str]=None):
    x_values = list(map(lambda v : v[xlabel], values))
    y_values = {}
    y_labels_to_delete = []
    for ylabel in ylabels:
        if not ylabel in values[0].keys(): 
            y_labels_to_delete.append(ylabel)
            continue
        y_values[ylabel] = list(map(lambda v : v[ylabel], values))
    for ylabel in y_labels_to_delete:
        ylabels.remove(ylabel)
    print(f'x_values {x_values}')
    print(f'y_values {y_values}')
    fig, ax = matplotlib.pyplot.subplots()
    for ylabel in ylabels:
        v = y_values[ylabel]
        ax.plot(x_values, v, label=ylabel)
    ax.set_xlabel(xlabel)
    ax.set_ylabel(ylabel)
    #ax.grid()
    ax.set_title(f'{variableName} output for different inputs')
    #ax.annotate('rabarber\nRaBarber', xytext=(0.9, 0.1), textcoords='axes fraction', bbox=dict(boxstyle='round'))
    p_text = []
    for k, v in parameters.items():
        if k in  ('measurement', 'interval'): continue
        if isinstance(v, float):
            p_text.append(f'{k} = {v:.2f}')
        else:
            p_text.append(f'{k} = {v}')
    ax.text(0.95, 0.05, '\n'.join(p_text), transform=ax.transAxes, bbox=dict(boxstyle='round'), multialignment='left', horizontalalignment='right', verticalalignment='bottom')
    ax.legend()
    if y2labels:
        y2_values = {}
        def clamp_temp(v):
            if v < 0 or v > 10000: return None
            return v
        for y2label in y2labels:
            y2_values[y2label] = list(map(lambda v : clamp_temp(v[y2label]), values))
        print(f'y2_values {y2_values}')
        ax2 = ax.twinx()
        ax2.set_ylim((2000, 8000))
        for ylabel in y2labels:
            v = y2_values[ylabel]
            ax2.plot(x_values, v, marker='.', label=ylabel)
        # ax2.set_xlabel(xlabel)
        ax2.set_ylabel(ylabel)
        ax2.legend()
    if filename:
        matplotlib.pyplot.savefig(filename)
    else:
        matplotlib.pyplot.show()
    
def plot_colors_bycct(values, parameters, ylabels, filename=None):
    fig, ax = colour.plotting.plot_chromaticity_diagram_CIE1931(standalone=False)
    mincct = values[0]['requested']
    maxcct = values[-1]['requested']
    ax.set_title(f'CCT measured for RGB LED, {int(mincct)} to {int(maxcct)}')
    for ylabel in ylabels:
        xvalues = []
        yvalues = []
        for value in values:
            cct_requested = value['requested']
            cct_measured = value[f'rgb_cct_{ylabel}']
            x, y = colour.CCT_to_xy(cct_measured)
            xvalues.append(x)
            yvalues.append(y)
        matplotlib.pyplot.plot(xvalues, yvalues, '.', label=ylabel)
    p_text = []
    for k, v in parameters.items():
        if k in  ('measurement', 'interval'): continue
        p_text.append(f'{k} = {v}')
    ax.text(0.95, 0.05, '\n'.join(p_text), transform=ax.transAxes, bbox=dict(boxstyle='round'), multialignment='left', horizontalalignment='right', verticalalignment='bottom')
    ax.legend()
#        matplotlib.pyplot.annotate(str(cct_requested), 
#            xy=(x, y),
#            xytext=(-50, 30),
#            textcoords='offset points',
#            arrowprops=dict(arrowstyle='->', connectionstyle='arc3, rad=-0.2'))
    if filename:
        matplotlib.pyplot.savefig(filename)
    else:
        matplotlib.pyplot.show()

def plot_colors(values, parameters, ylabels, filename : Optional[str]=None):
    #fig, ax = colour.plotting.plot_chromaticity_diagram_CIE1931(standalone=False)
    fig, ax = colour.plotting.plot_planckian_locus_in_chromaticity_diagram_CIE1931([], standalone=False)
    mincct = values[0]['requested']
    maxcct = values[-1]['requested']
    ax.set_title(f'CCT measured for RGB LED, {int(mincct)} to {int(maxcct)}')

    sRGB = colour.RGB_COLOURSPACES['sRGB']

    plotcoloridx = 0
    for value in values:
        plotcolor = f'C{plotcoloridx}'
        plotcoloridx += 1
        cct_requested = value['requested']
#       for ylabel in ylabels:
        x_req, y_req = colour.CCT_to_xy(cct_requested)
        rgb_xvalues = []
        rgb_yvalues = []
        cct_xvalues = []
        cct_yvalues = []
        for ylabel in ylabels:
            r = value[f'rgb_r_{ylabel}']
            g = value[f'rgb_g_{ylabel}']
            b = value[f'rgb_b_{ylabel}']
            RGB = np.array([r, g, b])
            XYZ = colour.RGB_to_XYZ(RGB, sRGB.whitepoint, sRGB.whitepoint, sRGB.matrix_RGB_to_XYZ)
            xy = colour.XYZ_to_xy(XYZ)
            x, y = xy
            rgb_xvalues.append(x)
            rgb_yvalues.append(y)
            cct_measured = value[f'rgb_cct_{ylabel}']
            x, y = colour.CCT_to_xy(cct_measured)
            cct_xvalues.append(x)
            cct_yvalues.append(y)
        matplotlib.pyplot.plot(rgb_xvalues, rgb_yvalues, color=plotcolor, marker='o', linestyle='', label=str(cct_requested))
        matplotlib.pyplot.plot(cct_xvalues, cct_yvalues, color=plotcolor, marker='.', linestyle='')
        matplotlib.pyplot.plot([x_req], [y_req], color=plotcolor, marker='X')
    p_text = []
    for k, v in parameters.items():
        if k in  ('measurement', 'interval'): continue
        p_text.append(f'{k} = {v}')
    ax.text(0.95, 0.05, '\n'.join(p_text), transform=ax.transAxes, bbox=dict(boxstyle='round'), multialignment='left', horizontalalignment='right', verticalalignment='bottom')
    ax.legend()
#        matplotlib.pyplot.annotate(str(cct_requested), 
#            xy=(x, y),
#            xytext=(-50, 30),
#            textcoords='offset points',
#            arrowprops=dict(arrowstyle='->', connectionstyle='arc3, rad=-0.2'))
    if filename:
        matplotlib.pyplot.savefig(filename)
    else:
        matplotlib.pyplot.show()
