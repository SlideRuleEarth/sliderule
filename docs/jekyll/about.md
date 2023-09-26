---
layout: single
title: About
author_profile: true
permalink: /about/
---

SlideRule is an open source framework for on-demand processing of science data in the cloud. The SlideRule project offers a new paradigm for NASA archival data management - rapid delivery of customizable on-demand data products, rather than hosting large volumes of standard derivative products that will inevitably be insufficient for some science applications.

The initial target application for SlideRule was processing the ICESat-2 point-cloud and atmospheric datasets for seasonal snow depth mapping and glacier research. The ICESat-2 SlideRule plugin offers customizable algorithms to process the archive of low-level data products from the NASA Ice Cloud and land Elevation Satellite-2 (ICESat-2) laser altimetry mission stored in AWS S3. The user defines a geographic area of interest and key processing parameters via an interactive web interface or the API, and SlideRule returns high-level surface elevation point cloud products in seconds to minutes, enabling rapid algorithm development, visualization and scientific interpretation.

Since then work on SlideRule has continued as we've added new algorithms that use ICESat-2 data, subsetting support for GEDI data, and the ability to sample and subset raster datasets (e.g. ArcticDEM, REMA, 3DEP, Landsat, and others) as a part of processing altimetry data. Future improvements include new algorithms supporting intra- and inter-misison altimetry crossover analysis, waveform processing and improved corrections and classification approaches for ICESat-2 data. Raster support will include efficient, server-side fusion, including sampling large raster time series data for sparse altimetry observations, point cloud and DEM co-registration, and altimetry advection for Lagrangian elevation change analysis.
