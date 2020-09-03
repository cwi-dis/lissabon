import matplotlib.pyplot as plt
import numpy as np

def plot_lines(values, parameters, xlabel, ylabels):
    x_values = list(map(lambda v : v[xlabel], values))
    y_values = {}
    for ylabel in ylabels:
        y_values[ylabel] = list(map(lambda v : v[ylabel], values))
    print(f'x_values {x_values}')
    print(f'y_values {y_values}')
    fig, ax = plt.subplots()
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
    plt.show()