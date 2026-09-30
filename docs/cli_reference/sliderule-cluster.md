---
title: sliderule-cluster
description: User guide and reference for the sliderule-cluster command line tool
---

`sliderule-cluster` is a command line tool for submitting requests to a SlideRule web service cluster. It can be used to check on the health and identity of a cluster, query cluster defaults, search for science data granules (CMR/Earthdata), and run any of SlideRule's dataframe-based processing APIs (e.g. `atl06x`, `atl03x`) from a shell or script.

# Quick Start

```{code-block} bash
# Which cluster am I talking to?
sliderule-cluster whoami

# What version is it running?
sliderule-cluster version

# Run an API request using a parameters file; prints the path of the output file
sliderule-cluster run atl06x --parms request.json
```

where `request.json` might look like:

```{code-block} json
:caption: request.json

{
  "poly": [
    {"lon": -108.3, "lat": 38.8},
    {"lon": -107.8, "lat": 38.8},
    {"lon": -107.8, "lat": 39.2},
    {"lon": -108.3, "lat": 39.2},
    {"lon": -108.3, "lat": 38.8}
  ],
  "t0": "2023-06-01T00:00:00Z",
  "t1": "2023-07-01T00:00:00Z"
}
```

```{note}
The parameters above are illustrative. See the SlideRule API reference for the full set of request parameters supported by each endpoint.
```

# Usage

```{code-block} text
sliderule-cluster <command> [options]
```

```{list-table}
:header-rows: 1

* - Command
  - Purpose
* - [`whoami`](#cmd-whoami)
  - Self-identification of the cluster; returns the cluster name
* - [`status`](#cmd-status)
  - Registration status of the cluster
* - [`version`](#cmd-version)
  - Cluster version information
* - [`defaults`](#cmd-defaults)
  - Default request parameter values used by the cluster
* - [`earthdata`](#cmd-earthdata)
  - Search for available science data resources
* - [`run`](#cmd-run)
  - Execute a dataframe-based API request and produce an output file
```

Run `sliderule-cluster --help` for a list of commands, or `sliderule-cluster <command> --help` for the options of a specific command.

(common-options)=
# Common Options

The following options are accepted by **every** command. They are attached to each subcommand, so they must appear **after** the command name:

```{list-table}
:header-rows: 1

* - Option
  - Default
  - Description
* - `--domain <domain>`
  - `slideruleearth.io`
  - Domain of the SlideRule service to connect to.
* - `--cluster <cluster>`
  - `sliderule`
  - Name of the cluster within the domain.
* - `--user_service`
  - False
  - Use dedicated user capacity rather than the public cluster.
* - `--verbose`
  - False
  - Turn on verbose log messages. Also causes errors to be raised with a full traceback (see [Error Handling](#error-handling)).
* - `--result <file>`
  - None
  - Write the command's result to the named file in addition to printing it.
```


---

# Command Reference

(cmd-whoami)=
## `whoami`
```{code-block} text
sliderule-cluster whoami [common options]
```

Asks the cluster to identify itself. Useful as a quick connectivity check and to confirm that `--domain`, `--cluster`, and `--user_service` are resolving to the service you expect.

Examples:
```{code-block} bash
# get the internal name of the public cluster
sliderule-cluster whoami

# get the internal name of the cluster running at sliderule.testsliderule.org
sliderule-cluster whoami --domain testsliderule.org --cluster sliderule
```

(cmd-status)=
## `status`
```{code-block} text
sliderule-cluster status [common options]
```

Reports the registration status of the cluster with the discovery service. When connected to a non-public service (for example when using `--user_service`), the request is scoped to that service.

Examples:
```{code-block} bash
# get the number of nodes registered on the public cluster
sliderule-cluster status

# get the number of nodes registered to the user's private service
sliderule-cluster status --user_service
```

(cmd-version)=
## `version`
```{code-block} text
sliderule-cluster version [common options]
```

Returns version information for the cluster (and the client used to reach it).

Examples:
```{code-block} bash
# get version of public cluster
sliderule-cluster version

# write version of public cluster to the file "version.json"
sliderule-cluster version --result version.json
```

(cmd-defaults)=
## `defaults`
```{code-block} text
sliderule-cluster defaults [common options]
```

Returns the cluster's default request parameter values. This is a helpful reference when deciding which parameters you actually need to set in a request.

Examples:
```{code-block} bash
# return defaults for public cluster
sliderule-cluster defaults
```

(cmd-earthdata)=
## `earthdata`
```{code-block} text
sliderule-cluster earthdata [options] [common options]
```

Queries the Earthdata catalog (CMR) through SlideRule to find the science data resources (granules) that match an area of interest and time range. Use it to preview what a `run` request will process.


```{list-table}
:header-rows: 1

* - Option
  - Default
  - Description
* - `--asset <str>`
  - `icesat2`
  - SlideRule asset name.
* - `--short_name <str>`
  - none
  - CMR dataset short name (e.g. `ATL03`).
* - `--poly <float...>`
  - none
  - Closed, counter-clockwise list of coordinate pairs defining the area of interest: `lon1 lat1 lon2 lat2 lon3 lat3 ... lon1 lat1`.
* - `--geojson <file>`
  - none
  - GeoJSON file defining the area of interest.
* - `--bbox <float...>`
  - none
  - Bounding box: `lon_ll lat_ll lon_ur lat_ur`.
* - `--t0 <str>`
  - none
  - Start time, ISO 8601: `YYYY-MM-DDTHH:MM:SSZ`.
* - `--t1 <str>`
  - none
  - Stop time, ISO 8601: `YYYY-MM-DDTHH:MM:SSZ`.
* - `--max_resources <int>`
  - none
  - Maximum number of resources the query may return. Queries exceeding this number return an error.
* - `--with_meta`
  - off
  - Return metadata along with the query results.
* - `--name_filter <str>`
  - none
  - Regular expression evaluated against the resource names returned by the query.
```

Only one area-of-interest option is used. If more than one is given, the precedence is `--poly`, then `--geojson`, then `--bbox`. Options that are not supplied are omitted from the request so that cluster defaults apply.

Examples:
```{code-block} bash
# Granules over a bounding box in June 2023
sliderule-cluster earthdata \
    --short_name ATL03 \
    --bbox -108.3 38.8 -107.8 39.2 \
    --t0 2023-06-01T00:00:00Z --t1 2023-07-01T00:00:00Z

# Only resources whose names match a pattern, with metadata
sliderule-cluster earthdata --geojson aoi.geojson --name_filter "ATL03_2023" --with_meta
```

(cmd-run)=
## `run`
```{code-block} text
sliderule-cluster run <api> --parms <file> [common options]
```

Executes a dataframe-based API request against the cluster and delivers the result as an output file (GeoParquet by default).


```{list-table}
:header-rows: 1

* - Argument / Option
  - Description
* - `<api>`
  - *(required, positional)* The endpoint being called, e.g. `atl06x`, `atl03x`.
* - `--parms <file>`
  - *(required)* Path to a file containing the JSON request parameters.
```

### Request parameters

The JSON in the parameters file is the same set of parameters that would be passed to the endpoint through any other SlideRule client. Refer to the API reference for the parameters supported by each endpoint. Use `sliderule-cluster defaults` to see the cluster's defaults.

### Output handling

Where the result ends up depends on the `output` block of your request parameters:

```{list-table}
:header-rows: 1

* - `output`
  - Behavior
* - Not supplied
  - The tool adds an `output` block requesting **GeoParquet**, written to a uniquely named file (`<uuid>.geoparquet`) in your system's temporary directory.
* - Supplied
  - The file is written out accourding to parameters supplied.
```

On completion the tool prints the location of the output file (a local path or a remote URL).

Examples:
```{code-block} bash
# Default output: a GeoParquet file in the temp directory
sliderule-cluster run atl06p --parms request.json

# Save the printed output location to a file for use by a script
sliderule-cluster run atl06p --parms request.json --result output_path.txt
```

---

# Additional Topics

(error-handling)=
### Error Handling

By default, errors are caught and reported as a single line:

```{code-block} text
Unhandled error: <message>
```

Pass `--verbose` to raise the full exception and traceback instead, which is the best first step when troubleshooting.

(saving-results)=
### Saving Results with `--result`

`--result <file>` writes the command's result string to a file. For `run` this is the output file's path or URL, which makes it convenient for chaining commands in shell scripts:

```{code-block} bash
sliderule-cluster run atl06p --parms request.json --result out.txt
OUT=$(cat out.txt)
```

### Environment and Connectivity Notes

- The tool connects to `--cluster` at `--domain`; the defaults target the public SlideRule service.
- `whoami` and `status` talk to the discovery service, and do not retry.
- Long-running `run` requests stream results back from the cluster; use `--verbose` to see progress messages.
