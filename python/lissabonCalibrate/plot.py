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
    
def plot_colors(values, parameters, ylabels):
    fig, ax = colour.plotting.plot_chromaticity_diagram_CIE1931(standalone=False)
    mincct = values[0]['requested']
    maxcct = values[-1]['requested']
    ax.set_title(f'CCT measured for RGB LED, {int(mincct)} to {int(maxcct)}')
    for ylabel in ylabels:
        xvalues = []
        yvalues = []
        for value in values:
            cct_requested = value['requested']
            cct_measured = value[ylabel]
            x, y = colour.CCT_to_xy(cct_measured)
            xvalues.append(x)
            yvalues.append(y)
        matplotlib.pyplot.plot(xvalues, yvalues, '.', label=ylabel)
    ax.legend()
#        matplotlib.pyplot.annotate(str(cct_requested), 
#            xy=(x, y),
#            xytext=(-50, 30),
#            textcoords='offset points',
#            arrowprops=dict(arrowstyle='->', connectionstyle='arc3, rad=-0.2'))
    matplotlib.pyplot.show()
