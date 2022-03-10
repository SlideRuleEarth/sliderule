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
import geopandas as gpd
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

        # dropdown menu for setting data release
        self.release = ipywidgets.Dropdown(
            options=['003', '004'],
            value='004',
            description='Release:',
            description_tooltip="Release: ICESat-2 data release",
            disabled=False,
            style=self.style,
        )

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

        # slider for setting confidence level for PE selection
        # eventually would be good to switch this to a IntRangeSlider with value=[0,4]
        self.confidence = ipywidgets.IntSlider(
            value=4,
            min=0,
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

        # slider for setting for YAPC along-track distance window
        self.yapc_win_x = ipywidgets.FloatSlider(
            value=21.0,
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

        # dropdown menu for setting map projection for polygons
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
        layer_options = ['3DEP','ASTER GDEM','ESRI imagery','RGI']
        self.layers = ipywidgets.SelectMultiple(
            options=layer_options,
            description='Add Layers:',
            description_tooltip=("Add Layers: contextual layers "
                "to add to leaflet map"),
            disabled=False,
            style=self.style,
        )

        # watch widgets for changes
        self.projection.observe(self.set_layers)

        # button and label for output file selection
        self.file = copy.copy(self.filename)
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

    # function for setting available map layers
    def set_layers(self, sender):
        """function for updating available map layers
        """
        if (self.projection.value == 'Global'):
            layer_options = ['3DEP','ASTER GDEM','ESRI imagery','RGI']
        elif (self.projection.value == 'North'):
            layer_options = ['ESRI imagery','ArcticDEM']
        elif (self.projection.value == 'South'):
            layer_options = ['LIMA','MOA','RAMP']
        self.layers.options=layer_options
        self.layers.value=[]

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
        self.file = self.loadlabel.value

    @property
    def filename(self):
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
        cmap_reverse_flag = '_r' if self.reverse.value else ''
        return cmap_reverse_flag

    @property
    def colormap(self):
        return self.cmap.value + self._r

# define projections for ipyleaflet tiles
projections = Bunch(
    # Alaska Polar Stereographic (WGS84)
    EPSG5936=Bunch(
        Basemap=dict(
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
        Basemap = dict(
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
        Imagery = dict(
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
            "crs": projections.EPSG5936.Basemap,
            "attribution": esri_attribution,
            "url": 'http://server.arcgisonline.com/ArcGIS/rest/services/Polar/Arctic_Ocean_Base/MapServer/tile/{z}/{y}/{x}'
        },
        "ArcticImagery": {
            "name": 'Esri.ArcticImagery',
            "crs": projections.EPSG5936.Basemap,
            "attribution": "Earthstar Geographics",
            "url": 'http://server.arcgisonline.com/ArcGIS/rest/services/Polar/Arctic_Imagery/MapServer/tile/{z}/{y}/{x}'
        },
        "ArcticOceanReference": {
            "name": 'Esri.ArcticOceanReference',
            "crs": projections.EPSG5936.Basemap,
            "attribution": esri_attribution,
            "url": 'http://server.arcgisonline.com/ArcGIS/rest/services/Polar/Arctic_Ocean_Reference/MapServer/tile/{z}/{y}/{x}'
        },
        "AntarcticBasemap": {
            "name": 'Esri.AntarcticBasemap',
            "crs": projections.EPSG3031.Basemap,
            "attribution":noaa_attribution,
            "url": 'https://tiles.arcgis.com/tiles/C8EMgrsFcRFL6LrL/arcgis/rest/services/Antarctic_Basemap/MapServer/tile/{z}/{y}/{x}'
        },
        "AntarcticImagery": {
            "name": 'Esri.AntarcticImagery',
            "crs": projections.EPSG3031.Imagery,
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
        Glaciers = ipyleaflet.WMSLayer(
            attribution=glims_attribution,
            layers='GLIMS_GLACIERS',
            format='image/png',
            url='https://www.glims.org/mapservice'
        )
    ),
    USGS = Bunch(
        Elevation = ipyleaflet.WMSLayer(
            attribution=usgs_3dep_attribution,
            layers="3DEPElevation:Hillshade Gray",
            format='image/png',
            url='https://elevation.nationalmap.gov/arcgis/services/3DEPElevation/ImageServer/WMSServer?',
        ),
        LIMA = ipyleaflet.WMSLayer(
            attribution=usgs_antarctic_attribution,
            layers="LIMA_Full_1km",
            format='image/png',
            transparent=True,
            url='https://nimbus.cr.usgs.gov/arcgis/services/Antarctica/USGS_EROS_Antarctica_Reference/MapServer/WmsServer',
            crs=projections.EPSG3031.LIMA
        ),
        MOA = ipyleaflet.WMSLayer(
            attribution=usgs_antarctic_attribution,
            layers="MOA_125_HP1_090_230",
            format='image/png',
            transparent=True,
            url='https://nimbus.cr.usgs.gov/arcgis/services/Antarctica/USGS_EROS_Antarctica_Reference/MapServer/WmsServer',
            crs=projections.EPSG3031.MOA
        ),
        RAMP = ipyleaflet.WMSLayer(
            attribution=usgs_antarctic_attribution,
            layers="Radarsat_Mosaic",
            format='image/png',
            transparent=True,
            url='https://nimbus.cr.usgs.gov/arcgis/services/Antarctica/USGS_EROS_Antarctica_Reference/MapServer/WmsServer',
            crs=projections.EPSG3031.RAMP
        )
    ),
    PGC = Bunch(
        ArcticDEM = ipyleaflet.WMSLayer(
            attribution=pgc_attribution,
            layers="0",
            format='image/png',
            transparent=True,
            url='http://elevation2.arcgis.com/arcgis/services/Polar/ArcticDEM/ImageServer/WMSserver',
            crs=projections.EPSG5936.ArcticDEM
        )
    )
)

# load basemap providers from dict
# https://github.com/geopandas/xyzservices/blob/main/xyzservices/lib.py
def _load_dict(data):
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
        kwargs.setdefault('attribution',False)
        kwargs.setdefault('zoom',False)
        kwargs.setdefault('scale',False)
        kwargs.setdefault('cursor',True)
        kwargs.setdefault('center',(39,-108))
        kwargs.setdefault('color','green')
        # create basemap in projection
        if (projection == 'Global'):
            self.map = ipyleaflet.Map(center=kwargs['center'],
                zoom=9, max_zoom=15,
                attribution_control=kwargs['attribution'],
                basemap=ipyleaflet.basemaps.Esri.WorldTopoMap)
            self.crs = 'EPSG:3857'
        elif (projection == 'North'):
            self.map = ipyleaflet.Map(center=(90,0),
                zoom=5, max_zoom=24,
                attribution_control=kwargs['attribution'],
                basemap=basemaps.Esri.ArcticOceanBase,
                crs=projections.EPSG5936.Basemap)
            self.map.add_layer(basemaps.Esri.ArcticOceanReference)
            self.crs = 'EPSG:5936'
        elif (projection == 'South'):
            self.map = ipyleaflet.Map(center=(-90,0),
                zoom=2, max_zoom=9,
                attribution_control=kwargs['attribution'],
                basemap=basemaps.Esri.AntarcticBasemap,
                crs=projections.EPSG3031.Basemap)
            self.crs = 'EPSG:3031'
        # initiate layers list
        self.layers = []
        # initialize selected feature
        self.selected_callback = None
        # add control for zoom
        if kwargs['zoom']:
            zoom_slider = ipywidgets.IntSlider(description='Zoom level:',
                min=self.map.min_zoom, max=self.map.max_zoom, value=self.map.zoom)
            ipywidgets.jslink((zoom_slider, 'value'), (self.map, 'zoom'))
            zoom_control = ipyleaflet.WidgetControl(widget=zoom_slider,
                position='topright')
            self.map.add_control(zoom_control)
        # add scale bar
        if kwargs['scale']:
            scale_control = ipyleaflet.ScaleControl(position='topright')
            self.map.add_control(scale_control)
        # add label for cursor position
        if kwargs['cursor']:
            self.cursor = ipywidgets.Label()
            label_control = ipyleaflet.WidgetControl(widget=self.cursor,
                position='bottomleft')
            self.map.add_control(label_control)
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
        self.map.add_control(draw_control)
        # initiate data and colorbars
        self.geojson = None
        self.tooltip = None
        self.hover_control = None
        self.fields = []
        self.colorbar = None

    # add map layers
    def add_layer(self, **kwargs):
        kwargs.setdefault('layers', [])
        # verify layers are iterable
        if isinstance(kwargs['layers'],(xyzservices.TileProvider,dict,str)):
            kwargs['layers'] = [kwargs['layers']]
        # add each layer to map
        for layer in kwargs['layers']:
            # try to add the layer
            try:
                if isinstance(layer,xyzservices.TileProvider):
                    self.map.add_layer(layer)
                elif isinstance(layer,dict):
                    self.map.add_layer(_load_dict(layer))
                elif isinstance(layer,str) and (layer == 'RGI'):
                    self.map.add_layer(layers.GLIMS.Glaciers)
                elif isinstance(layer,str) and (layer == '3DEP'):
                    self.map.add_layer(layers.USGS.Elevation)
                elif isinstance(layer,str) and (layer == 'ASTER GDEM'):
                    self.map.add_layer(basemaps.NASAGIBS.ASTER_GDEM_Greyscale_Shaded_Relief)
                elif isinstance(layer,str) and (self.crs == 'EPSG:3857') and (layer == 'ESRI imagery'):
                    self.map.add_layer(xyzservices.providers.Esri.WorldImagery)
                elif isinstance(layer,str) and (self.crs == 'EPSG:5936') and (layer == 'ESRI imagery'):
                    self.map.add_layer(basemaps.Esri.ArcticImagery)
                elif isinstance(layer,str) and (layer == 'ArcticDEM'):
                    self.map.add_layer(layers.PGC.ArcticDEM)
                elif isinstance(layer,str) and (layer == 'LIMA'):
                    self.map.add_layer(layers.USGS.LIMA)
                elif isinstance(layer,str) and (layer == 'MOA'):
                    self.map.add_layer(layers.USGS.MOA)
                elif isinstance(layer,str) and (layer == 'RAMP'):
                    self.map.add_layer(layers.USGS.RAMP)
                # try to add to layers attribute
                self.layers.append(layer)
            except ipyleaflet.LayerException as e:
                logging.info(f"Layer {layer} already on map")
                pass

    # remove map layers
    def remove_layer(self, **kwargs):
        kwargs.setdefault('layers', [])
        # verify layers are iterable
        if isinstance(kwargs['layers'],(xyzservices.TileProvider,dict,str)):
            kwargs['layers'] = [kwargs['layers']]
        # remove each layer to map
        for layer in kwargs['layers']:
            # try to remove layer from map
            try:
                if isinstance(layer,xyzservices.TileProvider):
                    self.map.remove_layer(layer)
                elif isinstance(layer,dict):
                    self.map.remove_layer(_load_dict(layer))
                elif isinstance(layer,str) and (layer == 'RGI'):
                    self.map.remove_layer(layers.GLIMS.Glaciers)
                elif isinstance(layer,str) and (layer == '3DEP'):
                    self.map.remove_layer(layers.USGS.Elevation)
                elif isinstance(layer,str) and (layer == 'ASTER GDEM'):
                    self.map.remove_layer(basemaps.NASAGIBS.ASTER_GDEM_Greyscale_Shaded_Relief)
                elif isinstance(layer,str) and (self.crs == 'EPSG:3857') and (layer == 'ESRI imagery'):
                    self.map.remove_layer(xyzservices.providers.Esri.WorldImagery)
                elif isinstance(layer,str) and (self.crs == 'EPSG:5936') and (layer == 'ESRI imagery'):
                    self.map.remove_layer(basemaps.Esri.ArcticImagery)
                elif isinstance(layer,str) and (layer == 'ArcticDEM'):
                    self.map.remove_layer(layers.PGC.ArcticDEM)
                elif isinstance(layer,str) and (layer == 'LIMA'):
                    self.map.remove_layer(layers.USGS.LIMA)
                elif isinstance(layer,str) and (layer == 'MOA'):
                    self.map.remove_layer(layers.USGS.MOA)
                elif isinstance(layer,str) and (layer == 'RAMP'):
                    self.map.remove_layer(layers.USGS.RAMP)
                # try to remove fromo layers attribute
                self.layers.remove(layer)
            except Exception as e:
                logging.critical(f"Could not remove layer {layer}")
                logging.error(traceback.format_exc())
                pass

    # handle cursor movements for label
    def handle_interaction(self, **kwargs):
        if (kwargs.get('type') == 'mousemove'):
            lat,lon = kwargs.get('coordinates')
            lon = sliderule.io.wrap_longitudes(lon)
            self.cursor.value = u"""Latitude: {d[0]:8.4f}\u00B0,
                Longitude: {d[1]:8.4f}\u00B0""".format(d=[lat,lon])

    # keep track of rectangles and polygons drawn on map
    def handle_draw(self, obj, action, geo_json):
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
            self.map.remove_layer(self.geojson)
            self.geojson = None
        # remove any prior instances of a colorbar
        if (action == 'deleted') and self.colorbar is not None:
            self.map.remove_control(self.colorbar)
            self.colorbar = None
        return self

    # add geodataframe data to leaflet map
    def GeoData(self, gdf, **kwargs):
        kwargs.setdefault('column_name', 'h_mean')
        kwargs.setdefault('cmap', 'viridis')
        kwargs.setdefault('vmin', None)
        kwargs.setdefault('vmax', None)
        kwargs.setdefault('radius', 1.0)
        kwargs.setdefault('fillOpacity', 0.5)
        kwargs.setdefault('weight', 3.0)
        kwargs.setdefault('stride', None)
        kwargs.setdefault('max_plot_points', 10000)
        kwargs.setdefault('tooltip', True)
        kwargs.setdefault('fields', ['index', 'h_mean', 'h_sigma',
            'dh_fit_dx', 'rms_misfit', 'w_surface_window_final',
            'delta_time', 'cycle', 'rgt', 'gt'])
        kwargs.setdefault('colorbar', True)
        # remove any prior instances of a data layer
        if self.geojson is not None:
            self.map.remove_layer(self.geojson)
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
        geodataframe['index'] = geodataframe.index
        # set colorbar limits to 2-98 percentile
        # if not using a defined plot range
        clim = gdf[column_name].quantile((0.02, 0.98)).values
        if kwargs['vmin'] is None:
            vmin = clim[0]
        else:
            vmin = np.copy(kwargs['vmin'])
        if kwargs['vmax'] is None:
            vmax = clim[-1]
        else:
            vmax = np.copy(kwargs['vmax'])
        # normalize data to be within vmin and vmax
        norm = colors.Normalize(vmin=vmin, vmax=vmax, clip=True)
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
        self.map.add_layer(self.geojson)
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
            self.add_colorbar(column_name=column_name, cmap=kwargs['cmap'], norm=norm)

    # functional call for setting colors of each point
    def style_callback(self, feature):
        return {
            "fillColor": feature["properties"]["color"],
            "color": feature["properties"]["color"],
        }

    # functional calls for hover events
    def handle_hover(self, feature, **kwargs):
        # combine html strings for hover tooltip
        self.tooltip.value = '<b>{0}:</b> {1}<br>'.format('id',feature['id'])
        self.tooltip.value += '<br>'.join(['<b>{0}:</b> {1}'.format(field,
            feature["properties"][field]) for field in self.fields])
        self.tooltip.layout.width = "220px"
        self.tooltip.layout.height = "300px"
        self.tooltip.layout.visibility = 'visible'
        self.map.add_control(self.hover_control)

    def handle_mouseout(self, _, content, buffers):
        event_type = content.get('type', '')
        if event_type == 'mouseout':
            self.tooltip.value = ''
            self.tooltip.layout.width = "0px"
            self.tooltip.layout.height = "0px"
            self.tooltip.layout.visibility = 'hidden'
            self.map.remove_control(self.hover_control)

    # functional calls for click events
    def handle_click(self, feature, **kwargs):
        if self.selected_callback != None:
            self.selected_callback(feature)

    def add_selected_callback(self, callback):
        self.selected_callback = callback

    # add colorbar widget to leaflet map
    def add_colorbar(self, **kwargs):
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
            self.map.remove_control(self.colorbar)
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
        self.map.add_control(self.colorbar)
        plt.close()
