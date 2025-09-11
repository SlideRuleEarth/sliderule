# Building a Plugin for SlideRule

2025-09-10

## Overview

This document walks through the process of building and installing a plugin for SlideRule.  Plugins extend the functionality of SlideRule through additional Lua scripts and shared objects.  The SlideRule executable searches for shared objects in a specific location and dynamically loads those objects early in the startup process.  Lua scripts installed in special locations are accessible from the SlideRule runtime as Lua extensions or APIs.

The purpose of a SlideRule plugin is to allow independent organizations to extend the functionality of SlideRule without needing to modify the SlideRule source code.  Independent development teams can build their plugins against the SlideRule code base and produce executable modules that can be loaded by the SlideRule executable at runtime.  These plugins can leverage the features of SlideRule (scalabilty, web services, and data processing), while providing specific functionality needed by their application.

:::{note}
All plugins loaded by the SlideRule executable on the public cluster must be coordinated by the SlideRule development team.
:::

## Components of a Plugin

A SlideRule plugin consists of three components: (1) a shared object, (2) lua extension scripts, (3) lua api scripts.

#### Shared Object

The plugin's shared object must be named `<plugin_name>.so` and export two "C" style functions: `void init<plugin_name>(void)` and `void deinit<plugin_name>(void)`.

At startup, the SlideRule executable loads all plugins and calls their initialization function (`init`).  When the SlideRule executable exits, the deinitialization (`deinit`) function for each loaded plugin is called.

The `init` function should perform at least one of the following:
* Extend the Lua runtime environment with functions namespaced under the `<pluging_name>`.  This is the primary method of extending the functionality of SlideRule.  SlideRule's functionality is executing through Lua scripts; therefore, by providing additional functions available to the Lua runtime, any API or extension in SlideRule has the ability to access that function.
> For example, the **atl24** plugin extends the Lua runtime with a function called `hdf5file`.  This function is accessed via `atl24.hdf5file` within any Lua script, and it takes an ATL24 GeoDataFrame and writes an ATL24 HDF5 file.
* Register *Raster* (`RasterObject::registerRaster`) which allows the raster to be sampled.
* Register *Asset Drivers* (`Asset:RegisterDriver`) which read different datasets and formats.
* Perform various other SlideRule library exposed configurations and registrations that extend the functionality at the C++ level in the code.

The `deinit` function should free all memory allocated by the `init` function.

#### Extension Scripts

Extension scripts in SlideRule are Lua scripts that can be called by the Lua API scripts to perform common functions.  They are also available at startup and can run in the background while SlideRule is running.  For example, an extension script could provide base64 encoding functions that would then be available to APIs that needed to encode their results in base64.  Or an extension script could run in the background and poll for an updated version of a remote file and then when it is found to be updated, it could download that file and reinitialize some part of SlideRule to use the updated file.

#### API Scripts

API scripts are the user-facing interface for all of SlideRule's functions.  When a user makes a processing request, that request is tied to a specific Lua API script that executes and performs the necessary functions to fulfill the processing request.  Any plugin which provides additional APIs, needs to provide those APIs as Lua API scripts.

## Installing a Plugin

#### Local Execution

The SlideRule executable looks in one place for all plugins: `/usr/local/etc/sliderule`.  Any shared object in that directory is loaded as a plugin.  Underneath that directory, any Lua script in `/usr/local/etc/sliderule/ext` is available as a Lua extension script, and any Lua script in `/usr/local/etc/sliderule/api` is available as a Lua API script.  Note - any script treated as an API script is considered to be directly executable upon request by a user.  For that reason, only scripts in the special directory are treated as API scripts; all other scripts are not directly accessible to users and must be called by API scripts.

#### Cloud Deployments

In the cloud deployment of SlideRule, two steps are necessary to make third-party plugins available to the SlideRule executable:
* The startup script for each AWS EC2 instance looks for plugins at `s3://sliderule/plugins` and copies any files there to the local instance's `/plugins` directory.
* The startup code within the SlideRule Docker container looks for a mounted `/plugins` directory and copies any files there to the internal `/usr/local/etc/sliderule` directory.
