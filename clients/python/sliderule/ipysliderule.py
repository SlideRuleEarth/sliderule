# Copyright (c) 2021, University of Washington
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# 1. Redistributions of source code must retain the above copyright notice,
#    this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright notice,
#    this list of conditions and the following disclaimer in the documentation
#    and/or other materials provided with the distribution.
#
# 3. Neither the name of the University of Washington nor the names of its
#    contributors may be used to endorse or promote products derived from this
#    software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY OF WASHINGTON AND CONTRIBUTORS
# “AS IS” AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
# TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE UNIVERSITY OF WASHINGTON OR
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import os
import io
import sys
import copy
import logging
import datetime
import traceback
import numpy as np
import collections.abc
import geopandas as gpd
import matplotlib.lines
import matplotlib.cm as cm
import matplotlib.colorbar
import matplotlib.pyplot as plt
import matplotlib.colors as colors
from traitlets.utils.bunch import Bunch
import sliderule.io

# imports with warnings if not present
try:
    import ipywidgets
except ModuleNotFoundError as e:
    sys.stderr.write("Warning: missing packages, some functions will throw an exception if called. (%s)\n" % (str(e)))
try:
    import tkinter.filedialog
except ModuleNotFoundError as e:
    sys.stderr.write("Warning: missing packages, some functions will throw an exception if called. (%s)\n" % (str(e)))
try:
    import IPython.display
except ModuleNotFoundError as e:
    sys.stderr.write("Warning: missing packages, some functions will throw an exception if called. (%s)\n" % (str(e)))

# imports that raise error if not present
try:
    import ipyleaflet
except ModuleNotFoundError as e:
    sys.stderr.write("Error: missing required packages. (%s)\n" % (str(e)))
    raise

try:
    import xyzservices
except ModuleNotFoundError as e:
    sys.stderr.write("Error: missing required packages. (%s)\n" % (str(e)))
    raise

class widgets:
    def __init__(self, **kwargs):
        # set default keyword options
        kwargs.setdefault('style', {})
        # set style
        self.style = copy.copy(kwargs['style'])

        # dropdown menu for setting asset
        self.asset = ipywidgets.Dropdown(
            options=['atlas-local', 'atlas-s3', 'nsidc-s3'],
            value='nsidc-s3',
            description='Asset:',
            description_tooltip="Asset: Location for SlideRule to get the data",
            disabled=False,
            style=self.style,
        )

        # dropdown menu for ICESat-2 product
        self.product = ipywidgets.Dropdown(
            options=['ATL03','ATL06','ATL08'],
            value='ATL03',
            description='Product:',
            description_tooltip=("Product: ICESat-2 data product "
                "\n\tATL03: Global Geolocated Photon Data"
                "\n\tATL06: Land Ice Height"
                "\n\tATL08: Land and Vegetation Height"),
            disabled=False,
            style=self.style,
        )

        # dropdown menu for setting data release
        self.release = ipywidgets.Dropdown(
            options=['003', '004', '005'],
            value='005',
            description='Release:',
            description_tooltip="Release: ICESat-2 data release",
            disabled=False,
            style=self.style,
        )

        self.start_date = ipywidgets.DatePicker(
            value=datetime.datetime(2018,10,13,0,0,0),
            description='Start Date',
            description_tooltip="Start Date: Starting date for CMR queries",
            disabled=False
        )

        self.end_date = ipywidgets.DatePicker(
            value=datetime.datetime.now(),
            description='End Date',
            description_tooltip="End Date: Ending date for CMR queries",
            disabled=False
        )

        # multiple select for photon classification
        class_options = ['atl03','quality','atl08','yapc']
        self.classification = ipywidgets.SelectMultiple(
            options=class_options,
            value=['atl03','atl08'],
            description='Classification:',
            description_tooltip=("Classification: Photon classification "
                "\n\tatl03: surface confidence"
                "\n\tquality: photon quality"
                "\n\tatl08: land classification"
                "\n\tyapc: yet another photon classifier"),
            disabled=False,
            style=self.style,
        )

        # watch classification widgets for changes
        self.classification.observe(self.set_classification)

        # dropdown menu for setting surface type
        # 0-land, 1-ocean, 2-sea ice, 3-land ice, 4-inland water
        surface_type_options = [
            'Land',
            'Ocean',
            'Sea ice',
            'Land ice',
            'Inland water'
        ]
        self.surface_type = ipywidgets.Dropdown(
            options=surface_type_options,
            value='Land',
            description='Surface Type:',
            description_tooltip=("Surface Type: ATL03 surface type for confidence "
                "classification\n\t0: land\n\t1: ocean\n\t2: sea ice\n\t"
                "3: land ice\n\t4: inland water"),
            disabled=False,
            style=self.style,
        )
        self.surface_type.layout.display = 'inline-flex'

        # slider for setting confidence level for PE selection
        # eventually would be good to switch this to a IntRangeSlider with value=[0,4]
        self.confidence = ipywidgets.IntSlider(
            value=4,
            min=-2,
            max=4,
            step=1,
            description='Confidence:',
            description_tooltip=("Confidence: ATL03 confidence level for surface "
                "type\n\t0: background\n\t1: within 10m\n\t2: low\n\t3: medium\n\t"
                "4: high"),
            disabled=False,
            continuous_update=False,
            orientation='horizontal',
            readout=True,
            readout_format='d',
            style=self.style,
        )
        self.confidence.layout.display = 'inline-flex'

        # selection for land surface classifications
        land_options = [
            'atl08_noise',
            'atl08_ground',
            'atl08_canopy',
            'atl08_top_of_canopy',
            'atl08_unclassified'
        ]
        self.land_class = ipywidgets.SelectMultiple(
            options=land_options,
            description='Land Class:',
            description_tooltip=("Land Class: ATL08 land classification "
                "for photons\n\t0: noise\n\t1: ground\n\t2: canopy\n\t"
                "3: top of canopy\n\t4: unclassified"),
            disabled=False,
            style=self.style,
        )
        self.land_class.layout.display = 'inline-flex'

        # selection for ATL03 quality flags
        quality_options = [
            'atl03_nominal',
            'atl03_possible_afterpulse',
            'atl03_possible_impulse_response',
            'atl03_possible_tep'
        ]
        self.quality = ipywidgets.SelectMultiple(
            value=['atl03_nominal'],
            options=quality_options,
            description='Quality:',
            description_tooltip=("Quality: ATL03 photon quality "
                "classification\n\t0: nominal\n\t"
                "1: possible afterpulse\n\t"
                "2: possible impulse response\n\t"
                "3: possible TEP"),
            disabled=False,
            style=self.style,
        )
        self.quality.layout.display = 'none'

        # slider for setting for YAPC kNN
        self.yapc_knn = ipywidgets.IntSlider(
            value=0,
            min=0,
            max=20,
            step=1,
            description='YAPC kNN:',
            description_tooltip=("YAPC kNN: number of nearest "
                "neighbors to use\n\t0: automatic selection "
                "of the number of neighbors"),
            disabled=False,
            continuous_update=False,
            orientation='horizontal',
            readout=True,
            readout_format='d',
            style=self.style,
        )
        self.yapc_knn.layout.display = 'none'

        # slider for setting for YAPC height window
        self.yapc_win_h = ipywidgets.FloatSlider(
            value=3.0,
            min=0.1,
            max=100,
            step=0.1,
            description='YAPC h window:',
            description_tooltip=("YAPC h window: window height "
                "used to filter the nearest neighbors"),
            disabled=False,
            continuous_update=False,
            orientation='horizontal',
            readout=True,
            readout_format='0.1f',
            style=self.style,
        )
        self.yapc_win_h.layout.display = 'none'

        # slider for setting for YAPC along-track distance window
        self.yapc_win_x = ipywidgets.FloatSlider(
            value=15.0,
            min=0.1,
            max=100,
            step=0.1,
            description='YAPC x window:',
            description_tooltip=("YAPC x window: window width "
                "used to filter the nearest neighbors"),
            disabled=False,
            continuous_update=False,
            orientation='horizontal',
            readout=True,
            readout_format='0.1f',
            style=self.style,
        )
        self.yapc_win_x.layout.display = 'none'

        # slider for setting for YAPC minimum photon events
        self.yapc_min_ph = ipywidgets.IntSlider(
            value=4,
            min=0,
            max=20,
            step=1,
            description='YAPC Minimum PE:',
            description_tooltip=("YAPC Minimum PE: minimum number of "
                "photons needed in an extent to calculate a YAPC score"),
            disabled=False,
            continuous_update=False,
            orientation='horizontal',
            readout=True,
            readout_format='d',
            style=self.style,
        )
        self.yapc_min_ph.layout.display = 'none'

        # slider for setting for YAPC weights for fit
        self.yapc_weight = ipywidgets.IntSlider(
            value=80,
            min=0,
            max=255,
            step=1,
            description='YAPC Weight:',
            description_tooltip=("YAPC Weight: minimum YAPC classification "
                "score of a photon to be used in the processing request"),
            disabled=False,
            continuous_update=False,
            orientation='horizontal',
            readout=True,
            readout_format='d',
            style=self.style,
        )
        self.yapc_weight.layout.display = 'none'

        # slider for setting length of ATL06-SR segment in meters
        self.length = ipywidgets.IntSlider(
            value=40,
            min=5,
            max=200,
            step=5,
            description='Length:',
            description_tooltip="Length: length of ATL06 segments in meters",
            disabled=False,
            continuous_update=False,
            orientation='horizontal',
            readout=True,
            readout_format='d',
            style=self.style,
        )

        # slider for setting step distance for successive segments in meters
        self.step = ipywidgets.IntSlider(
            value=20,
            min=5,
            max=200,
            step=5,
            description='Step:',
            description_tooltip="Step: step distance for successive ATL06 segments in meters",
            disabled=False,
            continuous_update=False,
            orientation='horizontal',
            readout=True,
            readout_format='d',
            style=self.style,
        )

        # slider for setting maximum number of iterations
        # (not including initial least-squares-fit selection)
        self.iteration = ipywidgets.IntSlider(
            value=1,
            min=0,
            max=20,
            step=1,
            description='Iterations:',
            description_tooltip=("Iterations: maximum number of iterations, "
                "not including initial least-squares-fit selection"),
            disabled=False,
            continuous_update=False,
            orientation='horizontal',
            readout=True,
            readout_format='d',
            style=self.style,
        )

        # slider for setting minimum along track spread
        self.spread = ipywidgets.FloatSlider(
            value=20,
            min=1,
            max=100,
            step=0.1,
            description='Spread:',
            description_tooltip=("Spread: minimum along track spread "
                "for valid segments in meters"),
            disabled=False,
            continuous_update=False,
            orientation='horizontal',
            readout=True,
            readout_format='0.1f',
            style=self.style,
        )
        # slider for setting minimum photon event (PE) count
        self.count = ipywidgets.IntSlider(
            value=10,
            min=1,
            max=50,
            step=1,
            description='PE Count:',
            description_tooltip=("PE Count: minimum number of photon events "
                "needed for valid segment fits"),
            disabled=False,
            continuous_update=False,
            orientation='horizontal',
            readout=True,
            readout_format='d',
            style=self.style,
        )

        # slider for setting minimum height of PE window in meters
        self.window = ipywidgets.FloatSlider(
            value=3,
            min=0.5,
            max=10,
            step=0.1,
            description='Window:',
            description_tooltip=("Window: minimum height the refined "
                "photon-selection window can shrink in meters"),
            disabled=False,
            continuous_update=False,
            orientation='horizontal',
            readout=True,
            readout_format='0.1f',
            style=self.style,
        )

        # slider for setting maximum robust dispersion in meters
        self.sigma = ipywidgets.FloatSlider(
            value=5,
            min=1,
            max=10,
            step=0.1,
            description='Sigma:',
            description_tooltip="Sigma: maximum robust dispersion in meters",
            disabled=False,
            continuous_update=False,
            orientation='horizontal',
            readout=True,
            readout_format='0.1f',
            style=self.style,
        )

        # dropdown menu for setting map projection
        # Global: Web Mercator (EPSG:3857)
        # North: Alaska Polar Stereographic (EPSG:5936)
        # South: Polar Stereographic South (EPSG:3031)
        projection_list = ['Global','North','South']
        self.projection = ipywidgets.Dropdown(
            options=projection_list,
            value='Global',
            description='Projection:',
            description_tooltip=("Projection: leaflet map projection\n\t"
                "Global: Web Mercator (EPSG:3857)\n\t"
                "Alaska Polar Stereographic (EPSG:5936)\n\t"
                "South: Polar Stereographic South (EPSG:3031)"),
            disabled=False,
            style=self.style,
        )

        # dropdown menu for selecting variable to draw on map
        variable_list = ['h_mean', 'h_sigma', 'dh_fit_dx', 'dh_fit_dy',
            'rms_misfit', 'w_surface_window_final', 'delta_time',
            'cycle', 'rgt']
        self.variable = ipywidgets.Dropdown(
            options=variable_list,
            value='h_mean',
            description='Variable:',
            description_tooltip="Variable: variable to display on leaflet map",
            disabled=False,
            style=self.style,
        )

        # all listed colormaps in matplotlib version
        cmap_set = set(cm.datad.keys()) | set(cm.cmaps_listed.keys())
        # colormaps available in this program
        # (no reversed, qualitative or miscellaneous)
        self.cmaps_listed = {}
        self.cmaps_listed['Perceptually Uniform Sequential'] = [
            'viridis','plasma','inferno','magma','cividis']
        self.cmaps_listed['Sequential'] = ['Greys','Purples',
            'Blues','Greens','Oranges','Reds','YlOrBr','YlOrRd',
            'OrRd','PuRd','RdPu','BuPu','GnBu','PuBu','YlGnBu',
            'PuBuGn','BuGn','YlGn']
        self.cmaps_listed['Sequential (2)'] = ['binary','gist_yarg',
            'gist_gray','gray','bone','pink','spring','summer',
            'autumn','winter','cool','Wistia','hot','afmhot',
            'gist_heat','copper']
        self.cmaps_listed['Diverging'] = ['PiYG','PRGn','BrBG',
            'PuOr','RdGy','RdBu','RdYlBu','RdYlGn','Spectral',
            'coolwarm', 'bwr','seismic']
        self.cmaps_listed['Cyclic'] = ['twilight',
            'twilight_shifted','hsv']
        # create list of available colormaps in program
        cmap_list = []
        for val in self.cmaps_listed.values():
            cmap_list.extend(val)
        # reduce colormaps to available in program and matplotlib
        cmap_set &= set(cmap_list)
        # dropdown menu for setting colormap
        self.cmap = ipywidgets.Dropdown(
            options=sorted(cmap_set),
            value='viridis',
            description='Colormap:',
            description_tooltip=("Colormap: matplotlib colormaps "
                "for displayed variable"),
            disabled=False,
            style=self.style,
        )

        # Reverse the colormap
        self.reverse = ipywidgets.Checkbox(
            value=False,
            description='Reverse Colormap',
            description_tooltip=("Reverse Colormap: reverse matplotlib "
                "colormap for displayed variable"),
            disabled=False,
            style=self.style,
        )

        # selection for adding layers to map
        layer_options = ['3DEP','ASTER GDEM','ESRI imagery','GLIMS','RGI']
        self.layers = ipywidgets.SelectMultiple(
            options=layer_options,
            description='Add Layers:',
            description_tooltip=("Add Layers: contextual layers "
                "to add to leaflet map"),
            disabled=False,
            style=self.style,
        )

        # selection for adding raster functions to map
        self.raster_functions = ipywidgets.Dropdown(
            options=[],
            description='Raster Layer:',
            description_tooltip=("Raster Layer: contextual raster "
                "functions to add to leaflet map"),
            disabled=False,
            style=self.style,
        )
        self.raster_functions.layout.display = 'none'

        # watch widgets for changes
        self.projection.observe(self.set_layers)
        self.layers.observe(self.set_raster_functions)

        # single plot widgets
        # single plot kind
        self.plot_kind = ipywidgets.Dropdown(
            options=['cycles','scatter'],
            value='scatter',
            description='Plot Kind:',
            description_tooltip=("Plot Kind: single track plot kind"
                "\n\tcycles: along-track plot showing all available cycles"
                "\n\tscatter: plot showing a single cycle possibly with ATL03"),
            disabled=False,
            style=self.style,
        )

        # single plot ATL03 classification
        self.plot_classification = ipywidgets.Dropdown(
            options = ["atl03", "atl08", "yapc", "none"],
            value = "atl08",
            description = "Classification",
            description_tooltip=("Classification: Photon classification "
                "\n\tatl03: surface confidence"
                "\n\tquality: photon quality"
                "\n\tatl08: land classification"
                "\n\tyapc: yet another photon classifier"),
            disabled = False,
        )

        # selection for reference ground track
        self.rgt = ipywidgets.Text(
            value='0',
            description="RGT:",
            description_tooltip="RGT: Reference Ground Track to plot",
            disabled=False
        )

        # cycle input text box
        self.cycle = ipywidgets.Text(
            value='0',
            description='Cycle:',
            description_tooltip="Cycle: Orbital cycle to plot",
            disabled=False
        )

        # selection for ground track
        ground_track_options = ["gt1l","gt1r","gt2l","gt2r","gt3l","gt3r"]
        self.ground_track = ipywidgets.Dropdown(
            options=ground_track_options,
            value='gt1l',
            description="Track:",
            description_tooltip="Track: Ground Track to plot",
            disabled=False
        )

        # watch plot kind widgets for changes
        self.plot_kind.observe(self.set_plot_kind)

        # button and label for output file selection
        self.file = copy.copy(self.atl06_filename)
        self.savebutton = ipywidgets.Button(
            description="Save As"
        )
        self.savelabel = ipywidgets.Text(
            value=self.file,
            disabled=False
        )
        # connect fileselect button with action
        self.savebutton.on_click(self.saveas_file)
        self.savelabel.observe(self.set_savefile)
        # create hbox of file selection
        if os.environ.get("DISPLAY"):
            self.filesaver = ipywidgets.HBox([
                self.savebutton,
                self.savelabel
            ])
        else:
            self.filesaver = copy.copy(self.savelabel)

        # button and label for input file selection
        self.loadbutton = ipywidgets.Button(
            description="File select"
        )
        self.loadlabel = ipywidgets.Text(
            value='',
            disabled=False
        )
        # connect fileselect button with action
        self.loadbutton.on_click(self.select_file)
        self.loadlabel.observe(self.set_loadfile)
        # create hbox of file selection
        if os.environ.get("DISPLAY"):
            self.fileloader = ipywidgets.HBox([
                self.loadbutton,
                self.loadlabel
            ])
        else:
            self.fileloader = copy.copy(self.loadlabel)

    # function for setting photon classifications
    def set_classification(self, sender):
        """function for setting photon classifications
        and updating the visibility of widgets
        """
        # atl03 photon confidence level
        if ('atl03' in self.classification.value):
            self.surface_type.layout.display = 'inline-flex'
            self.confidence.layout.display = 'inline-flex'
            self.confidence.layout.value = 4
        else:
            self.surface_type.layout.display = 'none'
            self.confidence.layout.display = 'none'
            self.confidence.layout.value = -2
        # atl03 photon quality flags
        if ('quality' in self.classification.value):
            self.quality.layout.display = 'inline-flex'
        else:
            self.quality.layout.display = 'none'
        # atl08 land classification flags
        if ('atl08' in self.classification.value):
            self.land_class.layout.display = 'inline-flex'
        else:
            self.land_class.layout.display = 'none'
        # yet another photon classifier (YAPC)
        if ('yapc' in self.classification.value):
            self.yapc_knn.layout.display = 'inline-flex'
            self.yapc_win_h.layout.display = 'inline-flex'
            self.yapc_win_x.layout.display = 'inline-flex'
            self.yapc_min_ph.layout.display = 'inline-flex'
            self.yapc_weight.layout.display = 'inline-flex'
        else:
            self.yapc_knn.layout.display = 'none'
            self.yapc_win_h.layout.display = 'none'
            self.yapc_win_x.layout.display = 'none'
            self.yapc_min_ph.layout.display = 'none'
            self.yapc_weight.layout.display = 'none'

    def set_atl03_defaults(self):
        """sets the default widget parameters for ATL03 requests
        """
        # default photon classifications
        class_options = ['atl03','atl08','yapc']
        self.classification.value = class_options
        # default ATL03 confidence
        self.confidence.value = -2
        # set land class options
        land_options = [
            'atl08_noise',
            'atl08_ground',
            'atl08_canopy',
            'atl08_top_of_canopy',
            'atl08_unclassified'
        ]
        self.land_class.value = land_options
        # set default ATL03 length
        self.length.value = 20
        # update variable list for ATL03 variables
        variable_list = ['atl03_cnf', 'atl08_class', 'cycle', 'delta_time',
            'distance', 'height', 'pair', 'quality_ph', 'rgt', 'sc_orient',
            'segment_dist', 'segment_id', 'track', 'yapc_score']
        self.variable.options = variable_list
        self.variable.value = 'height'
        # set default filename
        self.file = copy.copy(self.atl03_filename)
        self.savelabel.value = self.file

    def set_atl06_defaults(self):
        """sets the default widget parameters for ATL06 requests
        """
        # default photon classifications
        class_options = ['atl03','atl08']
        self.classification.value = class_options
        # default ATL06-SR confidence
        self.confidence.value = 4
        # set land class options
        self.land_class.value = []
        # set default ATL06-SR length
        self.length.value = 40
        # update variable list for ATL06-SR variables
        variable_list = ['h_mean', 'h_sigma', 'dh_fit_dx', 'dh_fit_dy',
            'rms_misfit', 'w_surface_window_final', 'delta_time',
            'cycle', 'rgt']
        self.variable.options = variable_list
        self.variable.value = 'h_mean'
        # set default filename
        self.file = copy.copy(self.atl06_filename)
        self.savelabel.value = self.file

    @property
    def time_start(self):
        """start time in ISO format
        """
        return self.start_date.value.isoformat()

    @property
    def time_end(self):
        """end time in ISO format
        """
        return self.end_date.value.isoformat()

    # function for setting available map layers
    def set_layers(self, sender):
        """function for updating available map layers
        """
        if (self.projection.value == 'Global'):
            layer_options = ['3DEP','ASTER GDEM','ESRI imagery','GLIMS','RGI']
        elif (self.projection.value == 'North'):
            layer_options = ['ESRI imagery','ArcticDEM']
        elif (self.projection.value == 'South'):
            layer_options = ['LIMA','MOA','RAMP','REMA']
        self.layers.options=layer_options
        self.layers.value=[]

    # function for setting available raster functions
    def set_raster_functions(self, sender):
        """sets available raster functions for image service layers
        """
        # available raster functions for each DEM
        if ('ArcticDEM' in self.layers.value):
            # set options for raster functions
            self.raster_functions.options = [
                "Aspect Map",
                "Hillshade Elevation Tinted",
                "Hillshade Gray",
                "Height Ellipsoidal",
                "Height Orthometric",
                "Slope Map",
                "Contour 25",
                "Contour Smoothed 25"]
            self.raster_functions.value = "Hillshade Gray"
            self.raster_functions.layout.display = 'inline-flex'
        elif ('REMA' in self.layers.value):
            # set options for raster functions
            self.raster_functions.options = [
                "Aspect Map",
                "Hillshade Elevation Tinted",
                "Hillshade Gray",
                "Height Orthometric",
                "Slope Degrees Map",
                "Contour 25",
                "Smooth Contour 25"]
            self.raster_functions.value = "Hillshade Gray"
            self.raster_functions.layout.display = 'inline-flex'
        else:
            # set options for raster functions
            self.raster_functions.options = []
            self.raster_functions.value = None
            self.raster_functions.layout.display = 'none'

    @property
    def rendering_rule(self):
        """sets rendering rule from a raster function value
        """
        return {"rasterFunction": self.raster_functions.value}

    # function for setting single track plot kind
    def set_plot_kind(self, sender):
        """function for setting single track plot kind
        """
        # atl03 photon confidence level
        if (self.plot_kind.value == 'scatter'):
            self.cycle.layout.display = 'inline-flex'
        elif (self.plot_kind.value == 'cycles'):
            self.cycle.layout.display = 'none'

    def saveas_file(self, b):
        """function for file save
        """
        IPython.display.clear_output()
        root = tkinter.Tk()
        root.withdraw()
        root.call('wm', 'attributes', '.', '-topmost', True)
        filetypes = (("HDF5 file", "*.h5"),
            ("netCDF file", "*.nc"),
            ("All Files", "*.*"))
        b.files = tkinter.filedialog.asksaveasfilename(
            initialfile=self.file,
            defaultextension='h5',
            filetypes=filetypes)
        self.savelabel.value = b.files
        self.file = b.files
        return self

    def set_savefile(self, sender):
        """return filename from saveas function
        """
        self.file = self.savelabel.value

    def select_file(self, b):
        """function for file selection
        """
        IPython.display.clear_output()
        root = tkinter.Tk()
        root.withdraw()
        root.call('wm', 'attributes', '.', '-topmost', True)
        filetypes = (("HDF5 file", "*.h5"),
            ("netCDF file", "*.nc"),
            ("All Files", "*.*"))
        b.files = tkinter.filedialog.askopenfilename(
            defaultextension='h5',
            filetypes=filetypes,
            multiple=False)
        self.loadlabel.value = b.files
        self.file = b.files
        return self

    def set_loadfile(self, sender):
        """return filename from file select function
        """
        self.file = self.loadlabel.value

    @property
    def atl03_filename(self):
        """default input and output file string
        """
        # get sliderule submission time
        now = datetime.datetime.now().strftime('%Y%m%d%H%M%S')
        args = (now, self.release.value)
        return "ATL03-SR_{0}_{1}.h5".format(*args)

    @property
    def atl06_filename(self):
        """default input and output file string
        """
        # get sliderule submission time
        now = datetime.datetime.now().strftime('%Y%m%d%H%M%S')
        args = (now, self.release.value)
        return "ATL06-SR_{0}_{1}.h5".format(*args)

    @property
    def format(self):
        """return the file format from file string
        """
        hdf = ('h5','hdf5','hdf')
        netcdf = ('nc','netcdf','nc3')
        if self.file.endswith(hdf):
            return 'hdf'
        elif self.file.endswith(netcdf):
            return 'netcdf'
        else:
            return ''

    @property
    def _r(self):
        """return string for reversed Matplotlib colormaps
        """
        cmap_reverse_flag = '_r' if self.reverse.value else ''
        return cmap_reverse_flag

    @property
    def colormap(self):
        """return string for Matplotlib colormaps
        """
        return self.cmap.value + self._r

    # click handler for individual photons
    def atl03_click_handler(self, feature):
        """handler for leaflet map clicks
        """
        GT = {"10":"gt1l","20":"gt1r","30":"gt2l","40":"gt2r","50":"gt3l","60":"gt3r"}
        try:
            self.rgt.value = str(feature["properties"]["rgt"])
            self.cycle.value = str(feature["properties"]["cycle"])
            gt = 20*feature["properties"]["track"] + 10*feature["properties"]["pair"] - 10
            self.ground_track.value = GT[str(gt)]
        except Exception as e:
            return
        else:
            return self

    # click handler for individual segments
    def atl06_click_handler(self, feature):
        """handler for leaflet map clicks
        """
        GT = {"10":"gt1l","20":"gt1r","30":"gt2l","40":"gt2r","50":"gt3l","60":"gt3r"}
        try:
            self.rgt.value = str(feature["properties"]["rgt"])
            self.cycle.value = str(feature["properties"]["cycle"])
            self.ground_track.value = GT[str(feature["properties"]["gt"])]
        except Exception as e:
            return
        else:
            return self

    # build sliderule ATL03 parameters using latest values from widget
    def build_atl03(self, **parms):
        """Build a SlideRule parameters dictionary for making ATL03 requests

        Parameters
        ----------
        parms : dict, dictionary of SlideRule parameters to update
        """
        # classification and checks
        # still return photon segments that fail checks
        parms["pass_invalid"] = True
        # default parameters for all cases
        parms["len"] = self.length.value
        # photon classification
        # atl03 photon confidence level
        if ('atl03' in self.classification.value):
            # surface type: 0-land, 1-ocean, 2-sea ice, 3-land ice, 4-inland water
            parms["srt"] = self.surface_type.index
            # confidence level for PE selection
            parms["cnf"] = self.confidence.value
        # atl03 photon quality flags
        if ('quality' in self.classification.value):
            # confidence level for PE selection
            parms["quality_ph"] = list(self.quality.value)
        # atl08 land classification flags
        if ('atl08' in self.classification.value):
            # ATL08 land surface classifications
            parms["atl08_class"] = list(self.land_class.value)
        # yet another photon classifier (YAPC)
        if ('yapc' in self.classification.value):
            parms["yapc"] = {}
            parms["yapc"]["knn"] = self.yapc_knn.value
            parms["yapc"]["min_ph"] = self.yapc_min_ph.value
            parms["yapc"]["win_h"] = self.yapc_win_h.value
            parms["yapc"]["win_x"] = self.yapc_win_x.value
        # return the parameter dictionary
        return parms

    # build sliderule ATL06 parameters using latest values from widget
    def build_atl06(self, **parms):
        """Build a SlideRule parameters dictionary for making ATL06 requests

        Parameters
        ----------
        parms : dict, dictionary of SlideRule parameters to update
        """
        # default parameters for all cases
        # length of ATL06-SR segment in meters
        parms["len"] = self.length.value
        # step distance for successive ATL06-SR segments in meters
        parms["res"] = self.step.value
        # maximum iterations, not including initial least-squares-fit selection
        parms["maxi"] = self.iteration.value
        # minimum along track spread
        parms["ats"] = self.spread.value
        # minimum PE count
        parms["cnt"] = self.count.value
        # minimum height of PE window in meters
        parms["H_min_win"] = self.window.value
        # maximum robust dispersion in meters
        parms["sigma_r_max"] = self.sigma.value
        # photon classification
        # atl03 photon confidence level
        if ('atl03' in self.classification.value):
            # surface type: 0-land, 1-ocean, 2-sea ice, 3-land ice, 4-inland water
            parms["srt"] = self.surface_type.index
            # confidence level for PE selection
            parms["cnf"] = self.confidence.value
        # atl03 photon quality flags
        if ('quality' in self.classification.value):
            # confidence level for PE selection
            parms["quality_ph"] = list(self.quality.value)
        # atl08 land classification flags
        if ('atl08' in self.classification.value):
            # ATL08 land surface classifications
            parms["atl08_class"] = list(self.land_class.value)
        # yet another photon classifier (YAPC)
        if ('yapc' in self.classification.value):
            parms["yapc"] = {}
            parms["yapc"]["score"] = self.yapc_weight.value
            parms["yapc"]["knn"] = self.yapc_knn.value
            parms["yapc"]["min_ph"] = self.yapc_min_ph.value
            parms["yapc"]["win_h"] = self.yapc_win_h.value
            parms["yapc"]["win_x"] = self.yapc_win_x.value
        # return the parameter dictionary
        return parms

    # update values from widget using sliderule parameters dictionary
    def set_values(self, parms):
        """Set widget values using a SlideRule parameters dictionary

        Parameters
        ----------
        parms : dict, dictionary of SlideRule parameters
        """
        # default parameters for all cases
        # length of ATL06-SR segment in meters
        if ('len' in parms.keys()):
            self.length.value = parms["len"]
        # step distance for successive ATL06-SR segments in meters
        if ('res' in parms.keys()):
            self.step.value = parms["res"]
        # maximum iterations, not including initial least-squares-fit selection
        if ('maxi' in parms.keys()):
            self.iteration.value = parms["maxi"]
        # minimum along track spread
        if ('ats' in parms.keys()):
            self.spread.value = parms["ats"]
        # minimum PE count
        if ('cnt' in parms.keys()):
            self.count.value = parms["cnt"]
        # minimum height of PE window in meters
        if ('H_min_win' in parms.keys()):
            self.window.value = parms["H_min_win"]
        # maximum robust dispersion in meters
        if ('sigma_r_max' in parms.keys()):
            self.sigma.value = parms["sigma_r_max"]
        # photon classification
        # atl03 photon confidence level
        # surface type: 0-land, 1-ocean, 2-sea ice, 3-land ice, 4-inland water
        if ('srt' in parms.keys()):
            self.surface_type.index = parms["srt"]
        # confidence level for PE selection
        if ('cnf' in parms.keys()):
            self.confidence.value = parms["cnf"]
        # atl03 photon quality flags
        if ('quality_ph' in parms.keys()):
            # confidence level for PE selection
            self.quality.value = parms["quality_ph"]
        # atl08 land classification flags
        if ('atl08_class' in parms.keys()):
            # ATL08 land surface classifications
            self.land_class.value = parms["atl08_class"]
        # yet another photon classifier (YAPC)
        if ('yapc' in parms.keys()) and ('score' in parms['yapc'].keys()):
            self.yapc_weight.value = parms["yapc"]["score"]
        if ('yapc' in parms.keys()) and ('knn' in parms['yapc'].keys()):
            self.yapc_knn.value = parms["yapc"]["knn"]
        if ('yapc' in parms.keys()) and ('min_ph' in parms['yapc'].keys()):
            self.yapc_min_ph.value = parms["yapc"]["min_ph"]
        if ('yapc' in parms.keys()) and ('win_h' in parms['yapc'].keys()):
            self.yapc_win_h.value = parms["yapc"]["win_h"]
        if ('yapc' in parms.keys()) and ('win_x' in parms['yapc'].keys()):
            self.yapc_win_x.value = parms["yapc"]["win_x"]
        # update values
        return self

    @property
    def RGT(self):
        """extract and verify Reference Ground Tracks (RGTs)
        """
        # extract RGT
        try:
            rgt = int(self.rgt.value)
        except:
            logging.critical(f"RGT {self.rgt.value} is invalid")
            return "0"
        # verify ground track values
        if (rgt >= 1) and (rgt <= 1387):
            return self.rgt.value
        else:
            logging.critical(f"RGT {self.rgt.value} is outside available range")
            return "0"

    @property
    def GT(self):
        """extract Ground Tracks (GTs)
        """
        ground_track_dict = dict(gt1l=10,gt1r=20,gt2l=30,gt2r=40,gt3l=50,gt3r=60)
        return ground_track_dict[self.ground_track.value]

    @property
    def PT(self):
        """extract Pair Tracks (PTs)
        """
        pair_track_dict = dict(gt1l=1,gt1r=1,gt2l=2,gt2r=2,gt3l=3,gt3r=3)
        return pair_track_dict[self.ground_track.value]

    @property
    def LR(self):
        """extract Left-Right from Pair Tracks (PTs)
        """
        lr_track_dict = dict(gt1l=0,gt1r=1,gt2l=0,gt2r=1,gt3l=0,gt3r=1)
        return lr_track_dict[self.ground_track.value]

    @property
    def orbital_cycle(self):
        """extract and verify ICESat-2 orbital cycles
        """
        #-- number of GPS seconds between the GPS epoch and ATLAS SDP epoch
        atlas_sdp_gps_epoch = 1198800018.0
        #-- number of GPS seconds since the GPS epoch for first ATLAS data point
        atlas_gps_start_time = atlas_sdp_gps_epoch + 24710205.39202261
        epoch1 = datetime.datetime(1980, 1, 6, 0, 0, 0)
        epoch2 = datetime.datetime(1970, 1, 1, 0, 0, 0)
        #-- get the total number of seconds since the start of ATLAS and now
        delta_time_epochs = (epoch2 - epoch1).total_seconds()
        atlas_UNIX_start_time = atlas_gps_start_time - delta_time_epochs
        present_time = datetime.datetime.now().timestamp()
        #-- divide total time by cycle length to get the maximum number of orbital cycles
        nc = np.ceil((present_time - atlas_UNIX_start_time) / (86400 * 91)).astype('i')
        all_cycles = [str(c + 1) for c in range(nc)]
        if (self.cycle.value in all_cycles):
            return self.cycle.value
        else:
            logging.critical(f"Cycle {self.cycle.value} is outside available range")
            return "0"

    def plot(self, gdf=None, **kwargs):
        """Creates plots of SlideRule outputs

        Parameters
        ----------
        gdf : obj, ATL06-SR GeoDataFrame
        ax : obj, matplotlib axes object
        kind : str, kind of plot to produce

            - 'scatter' : scatter plot of along-track heights
            - 'cycles' : time series plot for each orbital cycle
        cmap : str, matplotlib colormap
        title: str, title to use for the plot
        legend: bool, title to use for the plot
        legend_label: str, legend label type for 'cycles' plot
        legend_frameon: bool, use a background patch for legend
        column_name: str, GeoDataFrame column for 'cycles' plot
        atl03: obj, ATL03 GeoDataFrame for 'scatter' plot
        classification: str, ATL03 photon classification for scatter plot

            - 'atl03' : ATL03 photon confidence
            - 'atl08' : ATL08 photon-level land classification
            - 'yapc' : Yet Another Photon Classification photon-density
            - 'none' : no classification of photons
        cycle_start: int, beginning cycle for 'cycles' plot
        """
        # default keyword arguments
        kwargs.setdefault('ax', None)
        kwargs.setdefault('kind', 'cycles')
        kwargs.setdefault('cmap', 'viridis')
        kwargs.setdefault('title', None)
        kwargs.setdefault('legend', False)
        kwargs.setdefault('legend_label','date')
        kwargs.setdefault('legend_frameon',True)
        kwargs.setdefault('column_name', 'h_mean')
        kwargs.setdefault('atl03', None)
        kwargs.setdefault('classification', None)
        kwargs.setdefault('segments', True)
        kwargs.setdefault('cycle_start', 3)
        # variable to plot
        column = kwargs['column_name']
        # reference ground track and ground track
        RGT = int(self.RGT)
        GT = int(self.GT)
        # skip plot creation if no values are entered
        if (RGT == 0) or (GT == 0):
            return
        # create figure axis
        if kwargs['ax'] is None:
            fig,ax = plt.subplots(num=1, figsize=(8,6))
            fig.set_facecolor('white')
            fig.canvas.header_visible = False
        else:
            ax = kwargs['ax']
        # list of legend elements
        legend_elements = []
        # different plot types
        # cycles: along-track plot showing all available cycles
        # scatter: plot showing a single cycle possibly with ATL03
        if (kwargs['kind'] == 'cycles'):
            # for each unique cycles
            for cycle in gdf['cycle'].unique():
                # skip cycles with significant off pointing
                if (cycle < kwargs['cycle_start']):
                    continue
                # reduce data frame to RGT, ground track and cycle
                df = gdf[(gdf['rgt'] == RGT) & (gdf['gt'] == GT) &
                    (gdf['cycle'] == cycle)]
                if not any(df[column].values):
                    continue
                # plot reduced data frame
                l, = ax.plot(df['distance'].values,
                    df[column].values, marker='.', lw=0, ms=1.5)
                # create legend element for cycle
                if (kwargs['legend_label'] == 'date'):
                    label = df.index[0].strftime('%Y-%m-%d')
                elif (kwargs['legend_label'] == 'cycle'):
                    label = 'Cycle {0:0.0f}'.format(cycle)
                legend_elements.append(matplotlib.lines.Line2D([0], [0],
                    color=l.get_color(), lw=6, label=label))
            # add axes labels
            ax.set_xlabel('Along-Track Distance [m]')
            ax.set_ylabel(f'SlideRule {column}')
        elif (kwargs['kind'] == 'scatter'):
            # extract pair track parameters
            LR = int(self.LR)
            PT = int(self.PT)
            # extract orbital cycle parameters
            cycle = int(self.orbital_cycle)
            if (kwargs['atl03'] is not None):
                # reduce ATL03 data frame to RGT, ground track and cycle
                atl03 = kwargs['atl03'][(kwargs['atl03']['rgt'] == RGT) &
                    (kwargs['atl03']['track'] == PT) &
                    (kwargs['atl03']['pair'] == LR) &
                    (kwargs['atl03']['cycle'] == cycle)]
            if (kwargs['classification'] == 'atl08'):
                # noise, ground, canopy, top of canopy, unclassified
                colormap = np.array(['c','b','g','g','y'])
                classes = ['noise','ground','canopy','toc','unclassified']
                sc = ax.scatter(atl03.index.values, atl03["height"].values,
                    c=colormap[atl03["atl08_class"].values.astype('i')],
                    s=1.5, rasterized=True)
                for i,lab in enumerate(classes):
                    element = matplotlib.lines.Line2D([0], [0],
                        color=colormap[i], lw=6, label=lab)
                    legend_elements.append(element)
            elif (kwargs['classification'] == 'yapc'):
                sc = ax.scatter(atl03.index.values,
                    atl03["height"].values,
                    c=atl03["yapc_score"],
                    cmap=kwargs['cmap'],
                    s=1.5, rasterized=True)
                plt.colorbar(sc)
            elif (kwargs['classification'] == 'atl03'):
                # background, buffer, low, medium, high
                colormap = np.array(['y','c','b','g','m'])
                confidences = ['background','buffer','low','medium','high']
                # reduce data frame to photon classified for surface
                atl03 = atl03[atl03["atl03_cnf"] >= 0]
                sc = ax.scatter(atl03.index.values, atl03["height"].values,
                    c=colormap[atl03["atl03_cnf"].values.astype('i')],
                    s=1.5, rasterized=True)
                for i,lab in enumerate(confidences):
                    element = matplotlib.lines.Line2D([0], [0],
                        color=colormap[i], lw=6, label=lab)
                    legend_elements.append(element)
            elif (kwargs['atl03'] is not None):
                # plot all available ATL03 points as gray
                sc = ax.scatter(atl03.index.values, atl03["height"].values,
                    c='0.4', s=0.5, rasterized=True)
                legend_elements.append(matplotlib.lines.Line2D([0], [0],
                    color='0.4', lw=6, label='ATL03'))
            if kwargs['segments']:
                df = gdf[(gdf['rgt'] == RGT) & (gdf['gt'] == GT) &
                    (gdf['cycle'] == cycle)]
                # plot reduced data frame
                sc = ax.scatter(df.index.values, df["h_mean"].values,
                    c='red', s=2.5, rasterized=True)
                legend_elements.append(matplotlib.lines.Line2D([0], [0],
                    color='red', lw=6, label='ATL06-SR'))
            # add axes labels
            ax.set_xlabel('UTC')
            ax.set_ylabel('Height (m)')
        # add title
        if kwargs['title']:
            ax.set_title(kwargs['title'])
        # create legend
        if kwargs['legend']:
            lgd = ax.legend(handles=legend_elements, loc=3,
                frameon=kwargs['legend_frameon'])
        # set legend frame to solid white
        if kwargs['legend'] and kwargs['legend_frameon']:
            lgd.get_frame().set_alpha(1.0)
            lgd.get_frame().set_edgecolor('white')
        if kwargs['ax'] is None:
            # show the figure
            plt.tight_layout()

# define projections for ipyleaflet tiles
projections = Bunch(
    # Alaska Polar Stereographic (WGS84)
    EPSG5936=Bunch(
        ESRIBasemap=dict(
            name='EPSG:5936',
            custom=True,
            proj4def="""+proj=stere +lat_0=90 +lat_ts=90 +lon_0=-150 +k=0.994
                +x_0=2000000 +y_0=2000000 +datum=WGS84 +units=m +no_defs""",
            origin=[-2.8567784109255e+07, 3.2567784109255e+07],
            resolutions=[
                238810.813354,
                119405.406677,
                59702.7033384999,
                29851.3516692501,
                14925.675834625,
                7462.83791731252,
                3731.41895865639,
                1865.70947932806,
                932.854739664032,
                466.427369832148,
                233.213684916074,
                116.60684245803701,
                58.30342122888621,
                29.151710614575396,
                14.5758553072877,
                7.28792765351156,
                3.64396382688807,
                1.82198191331174,
                0.910990956788164,
                0.45549547826179,
                0.227747739130895,
                0.113873869697739,
                0.05693693484887,
                0.028468467424435
            ],
            bounds=[
                [-2623285.8808999992907047,-2623285.8808999992907047],
                [6623285.8803000003099442,6623285.8803000003099442]
            ]
        ),
        ArcticDEM=dict(
            name='EPSG:5936',
            custom=True,
            proj4def="""+proj=stere +lat_0=90 +lat_ts=90 +lon_0=-150 +k=0.994
                +x_0=2000000 +y_0=2000000 +datum=WGS84 +units=m +no_defs""",
            bounds=[[-1647720.5069000013,-2101522.3853999963],
                [5476281.493099999,5505635.614600004]]
        )
    )
    ,
    # Polar Stereographic South (WGS84)
    EPSG3031 = Bunch(
        ESRIBasemap = dict(
            name='EPSG:3031',
            custom=True,
            proj4def="""+proj=stere +lat_0=-90 +lat_ts=-71 +lon_0=0 +k=1
                +x_0=0 +y_0=0 +datum=WGS84 +units=m +no_defs""",
            origin=[-3.06361E7, 3.0636099999999993E7],
            resolutions=[
                67733.46880027094,
                33866.73440013547,
                16933.367200067736,
                8466.683600033868,
                4233.341800016934,
                2116.670900008467,
                1058.3354500042335,
                529.1677250021168,
                264.5838625010584,
            ],
            bounds=[
                [-4524583.19363305,-4524449.487765655],
                [4524449.4877656475,4524583.193633042]
            ]
        ),
        ESRIImagery = dict(
            name='EPSG:3031',
            custom=True,
            proj4def="""+proj=stere +lat_0=-90 +lat_ts=-71 +lon_0=0 +k=1
                +x_0=0 +y_0=0 +datum=WGS84 +units=m +no_defs""",
            origin=[-3.369955099203E7,3.369955101703E7],
            resolutions=[238810.81335399998,
                119405.40667699999,
                59702.70333849987,
                29851.351669250063,
                14925.675834625032,
                7462.837917312516,
                3731.4189586563907,
                1865.709479328063,
                932.8547396640315,
                466.42736983214803,
                233.21368491607402,
                116.60684245803701,
                58.30342122888621,
                29.151710614575396,
                14.5758553072877,
                7.28792765351156,
                3.64396382688807,
                1.82198191331174,
                0.910990956788164,
                0.45549547826179,
                0.227747739130895,
                0.113873869697739,
                0.05693693484887,
                0.028468467424435
            ],
            bounds=[
                [-9913957.327914657,-5730886.461772691],
                [9913957.327914657,5730886.461773157]
            ]
        ),
        REMA=dict(
            name='EPSG:3031',
            custom=True,
            proj4def="""+proj=stere +lat_0=-90 +lat_ts=-71 +lon_0=0 +k=1
                +x_0=0 +y_0=0 +datum=WGS84 +units=m +no_defs""",
        ),
        LIMA = dict(
            name='EPSG:3031',
            custom=True,
            proj4def="""+proj=stere +lat_0=-90 +lat_ts=-71 +lon_0=0 +k=1
                +x_0=0 +y_0=0 +datum=WGS84 +units=m +no_defs""",
            bounds=[[-2668275,-2294665],[2813725,2362335]]
        ),
        MOA = dict(
            name='EPSG:3031',
            custom=True,
            proj4def="""+proj=stere +lat_0=-90 +lat_ts=-71 +lon_0=0 +k=1
                +x_0=0 +y_0=0 +datum=WGS84 +units=m +no_defs""",
            bounds=[[-3174450,-2816050],[2867175,2406325]]
        ),
        RAMP = dict(
            name='EPSG:3031',
            custom=True,
            proj4def="""+proj=stere +lat_0=-90 +lat_ts=-71 +lon_0=0 +k=1
                +x_0=0 +y_0=0 +datum=WGS84 +units=m +no_defs""",
            bounds=[[-3174462.5,-2611137.5],[2867162.5,2406487.5]]
        )
    )
)

# attributions for the different basemaps and images
glims_attribution = """
Imagery reproduced from GLIMS and NSIDC (2005, updated 2018):
Global Land Ice Measurements from Space glacier database. (doi:10.7265/N5V98602)
"""
esri_attribution = """
Tiles &copy; Esri &mdash; Esri, DeLorme, NAVTEQ, TomTom, Intermap, iPC,
USGS, FAO, NPS, NRCAN, GeoBase, Kadaster NL, Ordnance Survey, Esri Japan,
METI, Esri China (Hong Kong), and the GIS User Community
"""
noaa_attribution = """
Imagery provided by NOAA National Centers for Environmental Information (NCEI);
International Bathymetric Chart of the Southern Ocean (IBCSO);
General Bathymetric Chart of the Oceans (GEBCO).
"""
usgs_3dep_attribution = """USGS National Map 3D Elevation Program (3DEP)"""
usgs_antarctic_attribution = """
U.S. Geological Survey (USGS), British Antarctic Survey (BAS),
National Aeronautics and Space Administration (NASA)
"""
pgc_attribution = """Esri, PGC, UMN, NSF, NGA, DigitalGlobe"""
nasa_attribution = """
Imagery provided by services from the Global Imagery Browse Services (GIBS),
operated by the NASA/GSFC/Earth Science Data and Information System
with funding provided by NASA/HQ.
"""

# define background ipyleaflet tile providers
providers = {
    "Esri": {
        "ArcticOceanBase": {
            "name": 'Esri.ArcticOceanBase',
            "crs": projections.EPSG5936.ESRIBasemap,
            "attribution": esri_attribution,
            "url": 'http://server.arcgisonline.com/ArcGIS/rest/services/Polar/Arctic_Ocean_Base/MapServer/tile/{z}/{y}/{x}'
        },
        "ArcticImagery": {
            "name": 'Esri.ArcticImagery',
            "crs": projections.EPSG5936.ESRIBasemap,
            "attribution": "Earthstar Geographics",
            "url": 'http://server.arcgisonline.com/ArcGIS/rest/services/Polar/Arctic_Imagery/MapServer/tile/{z}/{y}/{x}'
        },
        "ArcticOceanReference": {
            "name": 'Esri.ArcticOceanReference',
            "crs": projections.EPSG5936.ESRIBasemap,
            "attribution": esri_attribution,
            "url": 'http://server.arcgisonline.com/ArcGIS/rest/services/Polar/Arctic_Ocean_Reference/MapServer/tile/{z}/{y}/{x}'
        },
        "AntarcticBasemap": {
            "name": 'Esri.AntarcticBasemap',
            "crs": projections.EPSG3031.ESRIBasemap,
            "attribution":noaa_attribution,
            "url": 'https://tiles.arcgis.com/tiles/C8EMgrsFcRFL6LrL/arcgis/rest/services/Antarctic_Basemap/MapServer/tile/{z}/{y}/{x}'
        },
        "AntarcticImagery": {
            "name": 'Esri.AntarcticImagery',
            "crs": projections.EPSG3031.ESRIImagery,
            "attribution": "Earthstar Geographics",
            "url": 'http://server.arcgisonline.com/ArcGIS/rest/services/Polar/Antarctic_Imagery/MapServer/tile/{z}/{y}/{x}'
        },
    },
    "NASAGIBS": {
        "ASTER_GDEM_Greyscale_Shaded_Relief": {
            "name": "NASAGIBS.ASTER_GDEM_Greyscale_Shaded_Relief",
            "attribution": nasa_attribution,
            "url": "https://gibs.earthdata.nasa.gov/wmts/epsg3857/best/ASTER_GDEM_Greyscale_Shaded_Relief/default/GoogleMapsCompatible_Level12/{z}/{y}/{x}.jpg",
        }
    }
}

# define background ipyleaflet WMS layers
layers = Bunch(
    GLIMS = Bunch(
        GLACIERS = ipyleaflet.WMSLayer(
            name="GLIMS.GLACIERS",
            attribution=glims_attribution,
            layers='GLIMS_GLACIERS',
            format='image/png',
            transparent=True,
            url='https://www.glims.org/geoserver/GLIMS/wms'
        ),
        RGI = ipyleaflet.WMSLayer(
            name="GLIMS.RGI",
            attribution=glims_attribution,
            layers='RGI',
            format='image/png',
            transparent=True,
            url='https://www.glims.org/geoserver/GLIMS/wms'
        ),
        DCW = ipyleaflet.WMSLayer(
            name="GLIMS.DCW",
            attribution=glims_attribution,
            layers='dcw_glaciers',
            format='image/png',
            transparent=True,
            url='https://www.glims.org/geoserver/GLIMS/wms'
        ),
        WGI = ipyleaflet.WMSLayer(
            name="GLIMS.WGI",
            attribution=glims_attribution,
            layers='WGI_points',
            format='image/png',
            transparent=True,
            url='https://www.glims.org/geoserver/GLIMS/wms'
        )
    ),
    USGS = Bunch(
        Elevation = ipyleaflet.WMSLayer(
            name="3DEPElevation",
            attribution=usgs_3dep_attribution,
            layers="3DEPElevation:Hillshade Gray",
            format='image/png',
            url='https://elevation.nationalmap.gov/arcgis/services/3DEPElevation/ImageServer/WMSServer?',
        ),
        LIMA = ipyleaflet.WMSLayer(
            name="LIMA",
            attribution=usgs_antarctic_attribution,
            layers="LIMA_Full_1km",
            format='image/png',
            transparent=True,
            url='https://nimbus.cr.usgs.gov/arcgis/services/Antarctica/USGS_EROS_Antarctica_Reference/MapServer/WmsServer',
            crs=projections.EPSG3031.LIMA
        ),
        MOA = ipyleaflet.WMSLayer(
            name="MOA_125_HP1_090_230",
            attribution=usgs_antarctic_attribution,
            layers="MOA_125_HP1_090_230",
            format='image/png',
            transparent=False,
            url='https://nimbus.cr.usgs.gov/arcgis/services/Antarctica/USGS_EROS_Antarctica_Reference/MapServer/WmsServer',
            crs=projections.EPSG3031.MOA
        ),
        RAMP = ipyleaflet.WMSLayer(
            name="Radarsat_Mosaic",
            attribution=usgs_antarctic_attribution,
            layers="Radarsat_Mosaic",
            format='image/png',
            transparent=False,
            url='https://nimbus.cr.usgs.gov/arcgis/services/Antarctica/USGS_EROS_Antarctica_Reference/MapServer/WmsServer',
            crs=projections.EPSG3031.RAMP
        )
    ),
    PGC = Bunch()
)

# attempt to add PGC imageservice layers
try:
    layers.PGC.ArcticDEM = ipyleaflet.ImageService(
        name="ArcticDEM",
        attribution=pgc_attribution,
        format='jpgpng',
        transparent=True,
        url='https://elevation2.arcgis.com/arcgis/rest/services/Polar/ArcticDEM/ImageServer',
        crs=projections.EPSG5936.ArcticDEM
    )
    layers.PGC.REMA = ipyleaflet.ImageService(
        name="REMA",
        attribution=pgc_attribution,
        format='jpgpng',
        transparent=True,
        url='https://elevation2.arcgis.com/arcgis/rest/services/Polar/AntarcticDEM/ImageServer',
        crs=projections.EPSG3031.REMA
    )
except (NameError, AttributeError):
    layers.PGC.ArcticDEM = ipyleaflet.WMSLayer(
        name="ArcticDEM",
        attribution=pgc_attribution,
        layers="0",
        format='image/png',
        transparent=True,
        url='http://elevation2.arcgis.com/arcgis/services/Polar/ArcticDEM/ImageServer/WMSserver',
        crs=projections.EPSG5936.ArcticDEM
    )

# load basemap providers from dict
# https://github.com/geopandas/xyzservices/blob/main/xyzservices/lib.py
def _load_dict(data):
    """Creates a xyzservices TileProvider object from a dictionary
    """
    providers = Bunch()
    for provider_name in data.keys():
        provider = data[provider_name]
        if "url" in provider.keys():
            providers[provider_name] = xyzservices.lib.TileProvider(provider)
        else:
            providers[provider_name] = Bunch(
                {i: xyzservices.lib.TileProvider(provider[i]) for i in provider.keys()}
            )
    return providers

# create traitlets of basemap providers
basemaps = _load_dict(providers)

# draw ipyleaflet map
class leaflet:
    def __init__(self, projection, **kwargs):
        # set default keyword arguments
        kwargs.setdefault('map',None)
        kwargs.setdefault('attribution',False)
        kwargs.setdefault('zoom_control',False)
        kwargs.setdefault('scale_control',False)
        kwargs.setdefault('cursor_control',True)
        kwargs.setdefault('layer_control',True)
        kwargs.setdefault('center',(39,-108))
        kwargs.setdefault('color','green')
        # create basemap in projection
        if (projection == 'Global'):
            self.map = ipyleaflet.Map(center=kwargs['center'],
                zoom=9, max_zoom=15, world_copy_jump=True,
                attribution_control=kwargs['attribution'],
                basemap=ipyleaflet.basemaps.Esri.WorldTopoMap)
            self.crs = 'EPSG:3857'
        elif (projection == 'North'):
            self.map = ipyleaflet.Map(center=(90,0),
                zoom=5, max_zoom=24,
                attribution_control=kwargs['attribution'],
                basemap=ipyleaflet.basemaps.Esri.ArcticOceanBase,
                crs=ipyleaflet.projections.EPSG5936.ESRIBasemap)
            # add arctic ocean reference basemap
            reference = ipyleaflet.basemaps.Esri.ArcticOceanReference
            self.map.add(ipyleaflet.basemap_to_tiles(reference))
            self.crs = 'EPSG:5936'
        elif (projection == 'South'):
            self.map = ipyleaflet.Map(center=(-90,0),
                zoom=2, max_zoom=9,
                attribution_control=kwargs['attribution'],
                basemap=ipyleaflet.basemaps.Esri.AntarcticBasemap,
                crs=ipyleaflet.projections.EPSG3031.ESRIBasemap)
            self.crs = 'EPSG:3031'
        else:
            # use a predefined ipyleaflet map
            self.map = kwargs['map']
            self.crs = self.map.crs['name']
        # add control for layers
        if kwargs['layer_control']:
            self.layer_control = ipyleaflet.LayersControl(position='topleft')
            self.map.add(self.layer_control)
            self.layers = self.map.layers
        # add control for zoom
        if kwargs['zoom_control']:
            zoom_slider = ipywidgets.IntSlider(description='Zoom level:',
                min=self.map.min_zoom, max=self.map.max_zoom, value=self.map.zoom)
            ipywidgets.jslink((zoom_slider, 'value'), (self.map, 'zoom'))
            zoom_control = ipyleaflet.WidgetControl(widget=zoom_slider,
                position='topright')
            self.map.add(zoom_control)
        # add control for spatial scale bar
        if kwargs['scale_control']:
            scale_control = ipyleaflet.ScaleControl(position='topright')
            self.map.add(scale_control)
        # add control for cursor position
        if kwargs['cursor_control']:
            self.cursor = ipywidgets.Label()
            cursor_control = ipyleaflet.WidgetControl(widget=self.cursor,
                position='bottomleft')
            self.map.add(cursor_control)
            # keep track of cursor position
            self.map.on_interaction(self.handle_interaction)
        # add control for drawing polygons or bounding boxes
        draw_control = ipyleaflet.DrawControl(polyline={},circlemarker={},
            edit=False)
        shapeOptions = {'color':kwargs['color'],'fill_color':kwargs['color']}
        draw_control.rectangle = dict(shapeOptions=shapeOptions,
            metric=['km','m'])
        draw_control.polygon = dict(shapeOptions=shapeOptions,
            allowIntersection=False,showArea=True,metric=['km','m'])
        # create regions
        self.regions = []
        draw_control.on_draw(self.handle_draw)
        self.map.add(draw_control)
        # initialize data and colorbars
        self.geojson = None
        self.tooltip = None
        self.fields = []
        self.colorbar = None
        # initialize hover control
        self.hover_control = None
        # initialize selected feature
        self.selected_callback = None

    # add sliderule regions to map
    def add_region(self, regions, **kwargs):
        """adds SlideRule region polygons to leaflet maps
        """
        kwargs.setdefault('color','green')
        kwargs.setdefault('fillOpacity',0.8)
        kwargs.setdefault('weight',4)
        # for each sliderule region
        for region in regions:
            locations = [(p['lat'],p['lon']) for p in region]
            polygon = ipyleaflet.Polygon(
                locations=locations,
                color=kwargs['color'],
                fill_color=kwargs['color'],
                opacity=kwargs['fillOpacity'],
                weight=kwargs['weight'],
            )
            # add region to map
            self.map.add(polygon)
            # add to regions list
            self.regions.append(region)
        return self

    # add map layers
    def add_layer(self, **kwargs):
        """wrapper function for adding selected layers to leaflet maps
        """
        kwargs.setdefault('layers', [])
        kwargs.setdefault('rendering_rule', None)
        # verify layers are iterable
        if isinstance(kwargs['layers'],(xyzservices.TileProvider,dict,str)):
            kwargs['layers'] = [kwargs['layers']]
        elif not isinstance(kwargs['layers'],collections.abc.Iterable):
            kwargs['layers'] = [kwargs['layers']]
        # add each layer to map
        for layer in kwargs['layers']:
            # try to add the layer
            try:
                if isinstance(layer,xyzservices.TileProvider):
                    self.map.add(layer)
                elif isinstance(layer,dict):
                    self.map.add(_load_dict(layer))
                elif isinstance(layer,str) and (layer == 'GLIMS'):
                    self.map.add(layers.GLIMS.GLACIERS)
                elif isinstance(layer,str) and (layer == 'RGI'):
                    self.map.add(layers.GLIMS.RGI)
                elif isinstance(layer,str) and (layer == '3DEP'):
                    self.map.add(layers.USGS.Elevation)
                elif isinstance(layer,str) and (layer == 'ASTER GDEM'):
                    self.map.add(ipyleaflet.basemap_to_tiles(basemaps.NASAGIBS.ASTER_GDEM_Greyscale_Shaded_Relief))
                elif isinstance(layer,str) and (self.crs == 'EPSG:3857') and (layer == 'ESRI imagery'):
                    self.map.add(ipyleaflet.basemap_to_tiles(ipyleaflet.basemaps.Esri.WorldImagery))
                elif isinstance(layer,str) and (self.crs == 'EPSG:5936') and (layer == 'ESRI imagery'):
                    self.map.add(ipyleaflet.basemap_to_tiles(basemaps.Esri.ArcticImagery))
                elif isinstance(layer,str) and (layer == 'ArcticDEM'):
                    # set raster layer
                    im = layers.PGC.ArcticDEM
                    # remove previous versions of raster map
                    if (im in self.map.layers):
                        self.map.remove(im)
                    # update raster map rendering rule
                    im.rendering_rule = kwargs['rendering_rule']
                    self.map.add(im)
                elif isinstance(layer,str) and (layer == 'LIMA'):
                    self.map.add(layers.USGS.LIMA)
                elif isinstance(layer,str) and (layer == 'MOA'):
                    self.map.add(layers.USGS.MOA)
                elif isinstance(layer,str) and (layer == 'RAMP'):
                    self.map.add(layers.USGS.RAMP)
                elif isinstance(layer,str) and (layer == 'REMA'):
                    # set raster layer
                    im = layers.PGC.REMA
                    # remove previous versions of raster map
                    if (im in self.map.layers):
                        self.map.remove(im)
                    # update raster map rendering rule
                    im.rendering_rule = kwargs['rendering_rule']
                    self.map.add(im)
                else:
                    # simply attempt to add the layer or control
                    self.map.add(layer)
            except ipyleaflet.LayerException as e:
                logging.info(f"Layer {layer} already on map")
                pass
            except ipyleaflet.ControlException as e:
                logging.info(f"Control {layer} already on map")
                pass
            except Exception as e:
                logging.critical(f"Could add layer {layer}")
                logging.error(traceback.format_exc())
                pass

    # remove map layers
    def remove_layer(self, **kwargs):
        """wrapper function for removing selected layers from leaflet maps
        """
        kwargs.setdefault('layers', [])
        kwargs.setdefault('rendering_rule', None)
        # verify layers are iterable
        if isinstance(kwargs['layers'],(xyzservices.TileProvider,dict,str)):
            kwargs['layers'] = [kwargs['layers']]
        elif not isinstance(kwargs['layers'],collections.abc.Iterable):
            kwargs['layers'] = [kwargs['layers']]
        # remove each layer to map
        for layer in kwargs['layers']:
            # try to remove layer from map
            try:
                if isinstance(layer,xyzservices.TileProvider):
                    self.map.remove(layer)
                elif isinstance(layer,dict):
                    self.map.remove(_load_dict(layer))
                elif isinstance(layer,str) and (layer == 'GLIMS'):
                    self.map.remove(layers.GLIMS.GLACIERS)
                elif isinstance(layer,str) and (layer == 'RGI'):
                    self.map.remove(layers.GLIMS.RGI)
                elif isinstance(layer,str) and (layer == '3DEP'):
                    self.map.remove(layers.USGS.Elevation)
                elif isinstance(layer,str) and (layer == 'ASTER GDEM'):
                    self.map.remove(ipyleaflet.basemap_to_tiles(basemaps.NASAGIBS.ASTER_GDEM_Greyscale_Shaded_Relief))
                elif isinstance(layer,str) and (self.crs == 'EPSG:3857') and (layer == 'ESRI imagery'):
                    self.map.add(ipyleaflet.basemap_to_tiles(ipyleaflet.basemaps.Esri.WorldImagery))
                elif isinstance(layer,str) and (self.crs == 'EPSG:5936') and (layer == 'ESRI imagery'):
                    self.map.remove(ipyleaflet.basemap_to_tiles(basemaps.Esri.ArcticImagery))
                elif isinstance(layer,str) and (layer == 'ArcticDEM'):
                    self.map.remove(layers.PGC.ArcticDEM)
                elif isinstance(layer,str) and (layer == 'LIMA'):
                    self.map.remove(layers.USGS.LIMA)
                elif isinstance(layer,str) and (layer == 'MOA'):
                    self.map.remove(layers.USGS.MOA)
                elif isinstance(layer,str) and (layer == 'RAMP'):
                    self.map.remove(layers.USGS.RAMP)
                elif isinstance(layer,str) and (layer == 'REMA'):
                    self.map.remove(layers.PGC.REMA)
                else:
                    # simply attempt to remove the layer or control
                    self.map.remove(layer)
            except ipyleaflet.LayerException as e:
                logging.info(f"Layer {layer} already removed from map")
                pass
            except ipyleaflet.ControlException as e:
                logging.info(f"Control {layer} already removed from map")
                pass
            except Exception as e:
                logging.critical(f"Could not remove layer {layer}")
                logging.error(traceback.format_exc())
                pass

    # handle cursor movements for label
    def handle_interaction(self, **kwargs):
        """callback for handling mouse motion and setting location label
        """
        if (kwargs.get('type') == 'mousemove'):
            lat,lon = kwargs.get('coordinates')
            lon = sliderule.io.wrap_longitudes(lon)
            self.cursor.value = u"""Latitude: {d[0]:8.4f}\u00B0,
                Longitude: {d[1]:8.4f}\u00B0""".format(d=[lat,lon])

    # keep track of rectangles and polygons drawn on map
    def handle_draw(self, obj, action, geo_json):
        """callback for handling draw events and interactively
        creating SlideRule region objects
        """
        lon,lat = np.transpose(geo_json['geometry']['coordinates'])
        lon = sliderule.io.wrap_longitudes(lon)
        cx,cy = sliderule.io.centroid(lon,lat)
        wind = sliderule.io.winding(lon,lat)
        # set winding to counter-clockwise
        if (wind > 0):
            lon = lon[::-1]
            lat = lat[::-1]
        # create sliderule region from list
        region = sliderule.io.to_region(lon,lat)
        # append coordinates to list
        if (action == 'created'):
            self.regions.append(region)
        elif (action == 'deleted'):
            self.regions.remove(region)
        # remove any prior instances of a data layer
        if (action == 'deleted') and self.geojson is not None:
            self.map.remove(self.geojson)
            self.geojson = None
        # remove any prior instances of a colorbar
        if (action == 'deleted') and self.colorbar is not None:
            self.map.remove(self.colorbar)
            self.colorbar = None
        return self

    # add geodataframe data to leaflet map
    def GeoData(self, gdf, **kwargs):
        """Creates scatter plots of GeoDataFrames on leaflet maps

        Parameters
        ----------
        column_name : str, GeoDataFrame column to plot
        cmap : str, matplotlib colormap
        vmin : float, minimum value for normalization
        vmax : float, maximum value for normalization
        norm : obj, matplotlib color normalization object
        radius : float, radius of scatter plot markers
        fillOpacity : float, opacity of scatter plot markers
        weight : float, weight of scatter plot markers
        stride : int, number between successive array elements
        max_plot_points : int, total number of plot markers to render
        tooltip : bool, show hover tooltips
        fields : list, GeoDataFrame fields to show in hover tooltips
        colorbar : bool, show colorbar for rendered variable
        position : str, position of colorbar on leaflet map
        """
        kwargs.setdefault('column_name', 'h_mean')
        kwargs.setdefault('cmap', 'viridis')
        kwargs.setdefault('vmin', None)
        kwargs.setdefault('vmax', None)
        kwargs.setdefault('norm', None)
        kwargs.setdefault('radius', 1.0)
        kwargs.setdefault('fillOpacity', 0.5)
        kwargs.setdefault('weight', 3.0)
        kwargs.setdefault('stride', None)
        kwargs.setdefault('max_plot_points', 10000)
        kwargs.setdefault('tooltip', True)
        kwargs.setdefault('fields', self.default_atl06_fields())
        kwargs.setdefault('colorbar', True)
        kwargs.setdefault('position', 'topright')
        # remove any prior instances of a data layer
        if self.geojson is not None:
            self.map.remove(self.geojson)
        if kwargs['stride'] is not None:
            stride = np.copy(kwargs['stride'])
        elif (gdf.shape[0] > kwargs['max_plot_points']):
            stride = int(gdf.shape[0]//kwargs['max_plot_points'])
        else:
            stride = 1
        # sliced geodataframe for plotting
        geodataframe = gdf[slice(None,None,stride)]
        column_name = copy.copy(kwargs['column_name'])
        geodataframe['data'] = geodataframe[column_name]
        # set colorbar limits to 2-98 percentile
        # if not using a defined plot range
        clim = geodataframe['data'].quantile((0.02, 0.98)).values
        if kwargs['vmin'] is None:
            vmin = clim[0]
        else:
            vmin = np.copy(kwargs['vmin'])
        if kwargs['vmax'] is None:
            vmax = clim[-1]
        else:
            vmax = np.copy(kwargs['vmax'])
        # create matplotlib normalization
        if kwargs['norm'] is None:
            norm = colors.Normalize(vmin=vmin, vmax=vmax, clip=True)
        else:
            norm = copy.copy(kwargs['norm'])
        # normalize data to be within vmin and vmax
        normalized = norm(geodataframe['data'])
        # create HEX colors for each point in the dataframe
        geodataframe["color"] = np.apply_along_axis(colors.to_hex, 1,
            cm.get_cmap(kwargs['cmap'], 256)(normalized))
        # leaflet map point style
        point_style = {key:kwargs[key] for key in ['radius','fillOpacity','weight']}
        # convert to GeoJSON object
        self.geojson = ipyleaflet.GeoJSON(data=geodataframe.__geo_interface__,
            point_style=point_style, style_callback=self.style_callback)
        # add GeoJSON object to map
        self.map.add(self.geojson)
        # fields for tooltip views
        if kwargs['fields'] is None:
            self.fields = geodataframe.columns.drop(
                [geodataframe.geometry.name, "data", "color"])
        else:
            self.fields = copy.copy(kwargs['fields'])
        # add hover tooltips
        if kwargs['tooltip']:
            self.tooltip = ipywidgets.HTML()
            self.tooltip.layout.margin = "0px 20px 20px 20px"
            self.tooltip.layout.visibility = 'hidden'
            # create widget for hover tooltips
            self.hover_control = ipyleaflet.WidgetControl(widget=self.tooltip,
                position='bottomright')
            self.geojson.on_hover(self.handle_hover)
            self.geojson.on_msg(self.handle_mouseout)
            self.geojson.on_click(self.handle_click)
        # add colorbar
        if kwargs['colorbar']:
            self.add_colorbar(column_name=column_name,
                cmap=kwargs['cmap'], norm=norm,
                position=kwargs['position'])

    # functional call for setting colors of each point
    def style_callback(self, feature):
        """callback for setting marker colors
        """
        return {
            "fillColor": feature["properties"]["color"],
            "color": feature["properties"]["color"],
        }

    # functional calls for hover events
    def handle_hover(self, feature, **kwargs):
        """callback for creating hover tooltips
        """
        # combine html strings for hover tooltip
        self.tooltip.value = '<b>{0}:</b> {1}<br>'.format('id',feature['id'])
        self.tooltip.value += '<br>'.join(['<b>{0}:</b> {1}'.format(field,
            feature["properties"][field]) for field in self.fields])
        self.tooltip.layout.width = "220px"
        self.tooltip.layout.height = "300px"
        self.tooltip.layout.visibility = 'visible'
        self.map.add(self.hover_control)

    def handle_mouseout(self, _, content, buffers):
        """callback for removing hover tooltips upon mouseout
        """
        event_type = content.get('type', '')
        if event_type == 'mouseout':
            self.tooltip.value = ''
            self.tooltip.layout.width = "0px"
            self.tooltip.layout.height = "0px"
            self.tooltip.layout.visibility = 'hidden'
            self.map.remove(self.hover_control)

    # functional calls for click events
    def handle_click(self, feature, **kwargs):
        """callback for handling mouse clicks
        """
        if self.selected_callback != None:
            self.selected_callback(feature)

    def add_selected_callback(self, callback):
        """set callback for handling mouse clicks
        """
        self.selected_callback = callback

    # add colorbar widget to leaflet map
    def add_colorbar(self, **kwargs):
        """Creates colorbars on leaflet maps

        Parameters
        ----------
        column_name : str, GeoDataFrame column to plot
        cmap : str, matplotlib colormap
        norm : obj, matplotlib color normalization object
        alpha : float, opacity of colormap
        orientation : str, orientation of colorbar
        position : str, position of colorbar on leaflet map
        width : float, width of colorbar
        height : float, height of colorbar
        """
        kwargs.setdefault('column_name', 'h_mean')
        kwargs.setdefault('cmap', 'viridis')
        kwargs.setdefault('norm', None)
        kwargs.setdefault('alpha', 1.0)
        kwargs.setdefault('orientation', 'horizontal')
        kwargs.setdefault('position', 'topright')
        kwargs.setdefault('width', 6.0)
        kwargs.setdefault('height', 0.4)
        # remove any prior instances of a colorbar
        if self.colorbar is not None:
            self.map.remove(self.colorbar)
        # colormap for colorbar
        cmap = copy.copy(cm.get_cmap(kwargs['cmap']))
        # create matplotlib colorbar
        _, ax = plt.subplots(figsize=(kwargs['width'], kwargs['height']))
        cbar = matplotlib.colorbar.ColorbarBase(ax, cmap=cmap,
            norm=kwargs['norm'], alpha=kwargs['alpha'],
            orientation=kwargs['orientation'],
            label=kwargs['column_name'])
        cbar.solids.set_rasterized(True)
        cbar.ax.tick_params(which='both', width=1, direction='in')
        # save colorbar to in-memory png object
        png = io.BytesIO()
        plt.savefig(png, bbox_inches='tight', format='png')
        png.seek(0)
        # create output widget
        output = ipywidgets.Image(value=png.getvalue(), format='png')
        self.colorbar = ipyleaflet.WidgetControl(widget=output,
            transparent_bg=True, position=kwargs['position'])
        # add colorbar
        self.map.add(self.colorbar)
        plt.close()

    @staticmethod
    def default_atl03_fields():
        """List of ATL03 tooltip fields
        """
        return ['atl03_cnf', 'atl08_class', 'cycle', 'delta_time', 'height',
            'pair', 'rgt', 'segment_id', 'track', 'yapc_score']

    @staticmethod
    def default_atl06_fields():
        """List of ATL06-SR tooltip fields
        """
        return ['cycle', 'delta_time', 'dh_fit_dx', 'gt', 'h_mean',
            'h_sigma', 'rgt', 'rms_misfit', 'w_surface_window_final']
