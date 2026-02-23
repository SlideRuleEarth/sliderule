---
layout: single
title: About
author_profile: true
permalink: /about/
---

## What is SlideRule

SlideRule is a public web service that provides just-in-time cloud-based processing of Earth science data. It is accessible through REST-like APIs that return results directly to users. The goal of SlideRule is to provide researchers and downstream data systems with low-latency access to derived data products using processing parameters supplied at the time of the request.

## Who develops and maintains SlideRule

SlideRule is a collaboration between the University of Washington and NASA Goddard Space Flight Center with initial funding from the ICESat-2 program.  SlideRule runs in AWS us-west-2 and has access to a growing list of Earth science datasets, including but not limited to the ICESat-2, GEDI, and HLS Earthdata Cloud archives.  SlideRule is open-source under the BSD-3 Clause License, and is openly developed at [https://github.com/SlideRuleEarth/sliderule](https://github.com/SlideRuleEarth/sliderule).

## How can I start using SlideRule

The easiest way to get started with SlideRule is through the graphical interface available on this website. Navigate to the **Request** page using the top menu, take the guided tour, and begin exploring Earth science data immediately.

For advanced users, all SlideRule services are publicly available via HTTP GET and POST requests. To support easier and more robust API-level interactions, the SlideRule team provides:
- A Python client (`sliderule`, available on PyPI)
- A Node.js client (`sliderule`, available on npm)

Comprehensive documentation for the Python client is available at
https://docs.slideruleearth.io.

## What was the motivation for creating SlideRule

SlideRule was created to promote scientific discovery by lowering the barrier to accessing and analyzing Earth science data. To achieve this, the project introduces a new paradigm for NASA archival data management: the rapid delivery of customizable, on-demand data products and data-fusion services. This approach contrasts with traditional models that focus on distributing large volumes of static, mission-centric standard data products.

A service-based model for science data distribution promotes discovery in several key ways:

* **Broader Science Applications**
  By exposing processing parameters and pipeline configurations directly to users, SlideRule supports unforeseen science applications at no additional cost, using the same infrastructure that serves the mission’s primary science objectives.

* **Integrated Systems**
  Service-based architectures enable seamless integration with other data systems. These integrated services combine the strengths of multiple organizations to deliver products and capabilities that exceed what individual systems can provide independently.

* **Immediate Updates**
  Algorithm improvements, bug fixes, and new capabilities become available immediately. Historical data do not need to be reprocessed, updated datasets do not need to be redownloaded, and multiple algorithm versions can coexist.

* **Broader Access**
  SlideRule enables large-scale access to Earth science data for all users. The volume of data produced by modern Earth-observing satellites, along with the computational resources required to process it, presents a significant barrier to global-scale analysis for most users. As a result, many are limited to small, pre-defined spatial and temporal subsets of higher-level data products that may not meet their specific scientific needs.
