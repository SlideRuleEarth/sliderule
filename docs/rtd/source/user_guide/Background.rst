==========
Background
==========

ICESat-2
########

The `Ice Cloud and land Elevation Satellite-2 (ICESat-2) <https://icesat-2.gsfc.nasa.gov/>`_ is NASA's latest satellite laser altimeter.
The satellite was launched September 15, 2018 from Vandenberg Air Force Base in California onboard a `ULA Delta II rocket <https://youtu.be/jaIAqj-ReII>`_.
ICESat-2 has 1387 unique orbits that are repeated in an orbital cycle every 91 days.

The primary instrumentation onboard the ICESat-2 observatory is the Advanced Topographic Laser Altimeter System (ATLAS, a photon-counting laser altimeter).
ATLAS sends and receives data for 6 individual beams that are separated into three beam pairs.
The two paired beams are separated on the ground by 90 meters and the three beam pairs are separated by 3 kilometers.
Each beam pair consists of a weak beam and a strong beam, with the strong beam approximately four times brighter than weak.
The six beam setup was designed to allow the determination of both along-track and across-track slope simultaneously everywhere on the globe.

Each laser beam from the ATLAS instrument illuminates a spot on the ground.
The spots illuminated from strong beams are numbered 1, 3, and 5, and the spots illuminated from weak beams are numbered 2, 4, and 6.

The ICESat-2 observatory can be oriented in one of two positions with respect to the direction of travel.
In the forward orientation, the weak beams lead the strong beams and a weak beam is on the left edge of the beam pattern (gt1l).
This is reversed in the backward orientation, and the strong beams lead the weak beams with a strong beam on the left edge of the beam pattern.

.. table:: ATLAS beam mapping when in the forward orientation
    :align: center

    +-------------------+--------------------------+---------------+
    | ATLAS Spot Number | Ground track Designation | Beam Strength |
    +===================+==========================+===============+
    |         1         |           gt3r           |     Strong    |
    +-------------------+--------------------------+---------------+
    |         2         |           gt3l           |      Weak     |
    +-------------------+--------------------------+---------------+
    |         3         |           gt2r           |     Strong    |
    +-------------------+--------------------------+---------------+
    |         4         |           gt2l           |      Weak     |
    +-------------------+--------------------------+---------------+
    |         5         |           gt1r           |     Strong    |
    +-------------------+--------------------------+---------------+
    |         6         |           gt1l           |      Weak     |
    +-------------------+--------------------------+---------------+


ATL03 - Global Geolocated Photon Data
#####################################

The data from ATLAS and the secondary instrumentation onboard the ICESat-2 observatory (the global positioning system (GPS) and the star cameras)
are combined to create three primary measurements: the time of flight of a photon transmitted and received from ATLAS, the position of the satellite
in space, and the pointing vector of the satellite during the transmission of photons.
These three measurements are used to create `ATL03 <https://nsidc.org/data/atl03>`_, the geolocated photon product of ICESat-2.

ATL03 contains precise latitude, longitude and elevation for every received photon, arranged by beam in the along-track direction.
The structure of the ATL03 file has (at most) six beam groups, along with data describing the responses of the ATLAS instrument, ancillary data for correcting and transforming the ATL03 data, and a group of metadata.

Photon events can come to the ATLAS receiver in a few different ways:

- Many photons come from the sun either by reflecting off clouds or the land surface.  These photon events are spread in a random distribution along the telemetry band.  In ATL03, a large majority of these "background" photon events are classified, but some may be incorrectly classified as signal.
- Some photons are from the ATLAS instrument that have reflected off clouds. These photons can be clustered together or widely dispersed depending on the properties of the cloud and a few other variables.
- Some photons will be returns from the `Transmit Echo Path (TEP) <https://nsidc.org/sites/nsidc.org/files/technical-references/ATL03_Known_Issues_May2019.pdf>`_
- Some photons are from the ATLAS instrument that have reflected off the surface or vegetation (these are our signal photons).

The ATLAS instrument receives a vast amount of data and decides on-board whether or not to telemeter packets of received photons back to Earth.
ATLAS uses a digital elevation model (DEM) and a few simple rules when making this decision.
The photon events (PEs) that are returned are classified as being either signal or background for different surface types (land ice, sea ice, land, and ocean).
These PEs have a confidence level flag associated with it for each surface type:

- **-2**: possible Transmit Echo Path (TEP) photons
- **-1**: events not associated with a specific surface type
- **0**: noise
- **1**: buffer but algorithm classifies as background
- **2**: low
- **3**: medium
- **4**: high

There will be photons transmitted by the ATLAS instrument will never be recorded back.
The vast majority of these photons never reached the ATLAS instrument again (only about 10 out of the 10\ :sup:`14` photons transmitted are received), but some are not detected due to the "dead time" of the instrument.
This can create a bias towards the first photons that were received by the instrument, particularly for smooth and highly reflective surfaces.

The transmitted pulse is also not symmetric in time, which can introduce a bias when calculating average surfaces.
The magnitude of this bias depends on the shape of the transmitted waveform, the width of the window used to calculate the average surface, and the slope and roughness of the surface that broadens the return pulse.

ATL03 contains most of the data needed to create the higher level data products (such as the ATL06-SR land ice product).
With `SlideRule`, we will calculate the average elevation of segments for each beam.
In `SlideRule` the average segment elevations will not be corrected for transmit pulse shape biases or first photon biases as compared to the higher level data products.

Potential errors in the average surface heights:

1. **Sampling error**: average height estimates are based upon a random sampling of the surface heights, which might be skewed based on the horizontal distribution of PEs
2. **Background noise**: signal PEs are intermixed with the background PEs, and so there are random outliers which may affect the surface determination, particularly in conditions with high background rates and low surface reflectivity
3. **Complex topography**: the along-track linear fit will not always resolve complex surface topography
4. **Misidentified PEs**: the ATL03 processing will not always correctly identify the signal PEs
5. **First-photon bias**: this bias is inherent to photon-counting detectors and depends on the signal return strength
6. **Atmospheric forward scattering**: photons traveling through a cloudy atmosphere or a wind-blown snow event may be repeatedly scattered through small angles but still be reflected by the surface and be within the ATLAS field of view
7. **Subsurface scattering**: photons may be scattered many times within ice or snow before returning to the detector

More information about ATL03 can be found in the Algorithm Theoretical Basis Documents (ATBDs) provided by the ICESat-2 project:

- `ATL03: Global Geolocated Photon Data <https://nsidc.org/sites/nsidc.org/files/technical-references/ICESat2_ATL03_ATBD_r003.pdf>`_
- `ATL03g: Received Photon Geolocation <https://icesat-2.gsfc.nasa.gov/sites/default/files/page_files/ICESat2_ATL03g_ATBD_r002.pdf>`_
- `ATL03a: Atmospheric Delay Corrections <https://icesat-2.gsfc.nasa.gov/sites/default/files/page_files/I2_ATL03A_ATBD.pdf>`_

