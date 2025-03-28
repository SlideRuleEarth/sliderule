---
layout: single
title: About
author_profile: true
permalink: /about/
---

SlideRule is a collaboration between the University of Washington and NASA Goddard Space Flight Center to create an open source framework for on-demand processing of science data in the cloud. The SlideRule project offers a new paradigm for NASA archival data management - rapid delivery of customizable on-demand data products and data-fusion services.  This contrasts with existing approaches of hosting large volumes of standard derivative products that are inevitably insufficient for some science applications.

The initial target application for SlideRule was processing the ICESat-2 point-cloud and atmospheric datasets for seasonal snow depth mapping and glacier research. The ICESat-2 SlideRule module offers customizable algorithms to process the archive of low-level data products from the NASA Ice Cloud and land Elevation Satellite-2 (ICESat-2) laser altimetry mission stored in AWS S3. The user defines a geographic area of interest and key processing parameters via an interactive web interface or the API, and SlideRule returns high-level surface elevation point cloud products in seconds to minutes, enabling rapid algorithm development, visualization and scientific interpretation.

Since then, we've added new algorithms that use the ICESat-2 data, subsetting support for GEDI data, and the ability to dynamically sample and subset raster datasets (e.g. ArcticDEM, REMA, 3DEP, Landsat, and others) at points of interest. We are continuing to improve and enhance our functionality; this includes container runtime and orchestration, new algorithms supporting intra- and inter-mission altimetry crossover analysis, waveform processing, and improved corrections and classification approaches for Earth science data.
