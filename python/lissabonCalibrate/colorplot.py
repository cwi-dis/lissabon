from typing import List, Tuple
import matplotlib.pyplot
import colour
import colour.plotting

class ColorPlot:
    def __init__(self):
        fig, ax = colour.plotting.plot_planckian_locus_in_chromaticity_diagram_CIE1931([], standalone=False)
        ax.set_title("CCT color")
        self.fig = fig
        self.ax = ax

    def graph(self):
        cct_measured = self.data[0]['cct']
        x_dev, y_dev = colour.CCT_to_xy(cct_measured)

        r = self.data[0]['r']
        g = self.data[0]['g']
        b = self.data[0]['b']
        xyz = colour.RGB_to_XYZ([r, g, b], 'sRGB')
        x_py, y_py = colour.XYZ_to_xy(xyz)

        matplotlib.pyplot.plot([x_dev], [y_dev], '+', label="Device-converted", color="black")
        matplotlib.pyplot.plot([x_py], [y_py], '*', label="Python-converted", color="black")
        ax.legend()
        return
    
    def add(self, label : str, marker : str, color : str, linewidth : float, values : List[Tuple[float, float, float]]):
        x_values : List[float] = []
        y_values : List[float] = []
        for r, g, b in values:
            xyz = colour.RGB_to_XYZ([r, g, b], 'sRGB')
            x_py, y_py = colour.XYZ_to_xy(xyz)
            x_values.append(x_py)
            y_values.append(y_py)
        matplotlib.pyplot.plot(x_values, y_values, marker=marker, linewidth=linewidth, label=label, color=color)

    def show(self):
        self.ax.legend()
        matplotlib.pyplot.show()
