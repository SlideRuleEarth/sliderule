# Copyright (c) 2022, Jonathan Markel/UT Austin.
# 
# Redistribution and use in source and binary forms, with or without 
# modification, are permitted provided that the following conditions are met:
# 
# Redistributions of source code must retain the above copyright notice, 
# this list of conditions and the following disclaimer.
#
# Redistributions in binary form must reproduce the above copyright notice, 
# this list of conditions and the following disclaimer in the documentation 
# and/or other materials provided with the distribution.
#
# Neither the name of the copyright holder nor the names of its 
# contributors may be used to endorse or promote products derived from this 
# software without specific prior written permission. 
# 
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED 
# TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR 
# PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR 
# CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, 
# EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, 
# PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; 
# OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR '
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF 
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

import numpy as np

def photon_refraction(W, Z, ref_az, ref_el,
                      n1=1.00029, n2=1.34116):
    '''
    ICESat-2 refraction correction implented as outlined in Parrish, et al. 
    2019 for correcting photon depth data.

    Highly recommended to reference elevations to geoid datum to remove sea
    surface variations.

    https://www.mdpi.com/2072-4292/11/14/1634

    Code Author: 
    Jonathan Markel
    Graduate Research Assistant
    3D Geospatial Laboratory
    The University of Texas at Austin
    jonathanmarkel@gmail.com

    Parameters
    ----------
    W : float, or nx1 array of float
        Elevation of the water surface.

    Z : nx1 array of float
        Elevation of seabed photon data. Highly recommend use of geoid heights.

    ref_az : nx1 array of float
        Photon-rate reference photon azimuth data. Should be pulled from ATL03
        data parameter 'ref_azimuth'. Must be same size as seabed Z array.

    ref_el : nx1 array of float
        Photon-rate reference photon azimuth data. Should be pulled from ATL03
        data parameter 'ref_elev'. Must be same size as seabed Z array.

    n1 : float, optional
        Refractive index of air. The default is 1.00029.

    n2 : float, optional
        Refractive index of water. Recommended to use 1.34116 for saltwater 
        and 1.33469 for freshwater. The default is 1.34116.

    Returns
    -------
    dE : nx1 array of float
        Easting offset of seabed photons.

    dN : nx1 array of float
        Northing offset of seabed photons.

    dZ : nx1 array of float
        Vertical offset of seabed photons.

    '''

    # compute uncorrected depths
    D = W - Z
    H = 496  # mean orbital altitude of IS2, km
    Re = 6371  # mean radius of Earth, km

    # angle of incidence (wout Earth curvature)
    theta_1_ = (np.pi / 2) - ref_el

    # # ignoring curvature correction based on correspondence with the author
    # # incidence correction for earths curvature - eq 13
    # delta_theta_EC = np.arctan(H * np.tan(theta_1_) / Re)

    # angle of incidence
    theta_1 = theta_1_ # + delta_theta_EC

    # angle of refraction
    theta_2 = np.arcsin(n1 * np.sin(theta_1) / n2)  # eq 1

    phi = theta_1 - theta_2

    # uncorrected slant range to the uncorrected seabed photon location
    S = D / np.cos(theta_1)  # eq 3

    # corrected slant range
    R = S * n1 / n2  # eq 2

    P = np.sqrt(R**2 + S**2 - 2*R*S*np.cos(theta_1 - theta_2))  # eq 6

    gamma = (np.pi / 2) - theta_1  # eq 4

    alpha = np.arcsin(R * np.sin(phi) / P)  # eq 5

    beta = gamma - alpha  # eq 7

    # cross-track offset
    dY = P * np.cos(beta)  # eq 8

    # vertical offset
    dZ = P * np.sin(beta)  # eq 9

    kappa = ref_az

    # UTM offsets
    dE = dY * np.sin(kappa)  # eq 10
    dN = dY * np.cos(kappa)  # eq 11

    return dE, dN, dZ