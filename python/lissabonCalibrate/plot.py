import matplotlib.pyplot as plt
import numpy as np

def plot_lines(values, xlabel, ylabels):
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
    ax.legend()
    plt.show()