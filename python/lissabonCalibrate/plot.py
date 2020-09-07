import numpy as np
import matplotlib.pyplot
import colour
import colour.plotting

def plot_lines(values, parameters, xlabel, ylabels):
    x_values = list(map(lambda v : v[xlabel], values))
    y_values = {}
    for ylabel in ylabels:
        y_values[ylabel] = list(map(lambda v : v[ylabel], values))
    print(f'x_values {x_values}')
    print(f'y_values {y_values}')
    fig, ax = matplotlib.pyplot.subplots()
    for ylabel in ylabels:
        v = y_values[ylabel]
        ax.plot(x_values, v, label=ylabel)
    ax.set_xlabel(xlabel)
    ax.set_ylabel(ylabel)
    #ax.grid()
    ax.set_title('LUX output for different inputs')
    #ax.annotate('rabarber\nRaBarber', xytext=(0.9, 0.1), textcoords='axes fraction', bbox=dict(boxstyle='round'))
    p_text = []
    for k, v in parameters.items():
        if k in  ('measurement', 'interval'): continue
        p_text.append(f'{k} = {v}')
    ax.text(0.95, 0.05, '\n'.join(p_text), transform=ax.transAxes, bbox=dict(boxstyle='round'), multialignment='left', horizontalalignment='right', verticalalignment='bottom')
    ax.legend()
    matplotlib.pyplot.show()
    
def plot_colors_bycct(values, parameters, ylabels):
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
    matplotlib.pyplot.show()

def plot_colors(values, parameters, ylabels):
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
            XYZ = colour.RGB_to_XYZ(RGB, sRGB.whitepoint, sRGB.whitepoint, sRGB.RGB_to_XYZ_matrix)
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
    matplotlib.pyplot.show()
