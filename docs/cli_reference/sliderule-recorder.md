---
title: sliderule-recorder
description: User guide and reference for the sliderule-recorder command line tool
---

`sliderule-recorder` is a command line tool for generating usage statistics from SlideRule telemetry and alert data. It queries the SlideRule Athena/Glue tables for a specified time range, reports usage by source location, client, endpoint, and status code, and can optionally generate global usage and area-of-interest maps.

```{admonition} AWS Environment Required
:class: attention

The tool accesses the SlideRule telemetry infrastructure through AWS Athena, Glue, and S3. It is intended to run in an environment with AWS credentials and access to the configured SlideRule reporting resources.
```

# Quick Start

```{code-block} bash

# Generate a usage report for a specific date range
sliderule-recorder --start "2026-01-10" --end "2026-01-29"

# Generate a report from a start date through the current time
sliderule-recorder --start "2026-01-10"

# Generate the report and create usage and AOI maps
sliderule-recorder --start "2026-01-10" --grid

```

The `--start` option is required. If `--end` is not supplied, the end of the requested range defaults to the time when the program is started.

# Usage

```{code-block} text
sliderule-recorder --start <datetime> [options]
```

Generates a usage report for the specified time range.

The start and end values are passed to Python's ISO datetime parser. The tool constructs an Athena query covering the requested date range and handles telemetry timestamps represented either as Unix timestamps or ISO 8601 timestamps.

The following options are available:

```{list-table}
:header-rows: 1

* - Option
  - Default
  - Description

* - `--start <datetime>`
  - Required
  - Beginning of the reporting period. Parsed as an ISO datetime string.

* - `--end <datetime>`
  - Current time
  - End of the reporting period. Parsed as an ISO datetime string.

* - `--grid`
  - False
  - Generate the usage and AOI grid images.

* - `--grid_aoi <path>`
  - `/data/web/sliderule_aoi_grid.png`
  - Output filename for the area-of-interest grid image.

* - `--grid_usage <path>`
  - `/data/web/sliderule_usage_grid.png`
  - Output filename for the usage-by-location grid image.

* - `--geonames <path>`
  - `/data/geopy/cities500.txt`
  - GeoNames `cities500.txt` database used for offline location lookup when generating the usage grid.

* - `--country_info <path>`
  - `/data/geopy/countryInfo.txt`
  - GeoNames `countryInfo.txt` database used for country-name to country-code lookup when generating the usage grid.

```

Run `sliderule-recorder --help` for the complete command-line help.

# Reporting

The tool queries the SlideRule telemetry and alerts tables for the requested time range. It ensures that the required S3 partitions are represented in the corresponding Glue tables before executing the Athena queries.

The report includes:

```{list-table}
:header-rows: 1

* - Statistic
  - Description

* - `Start`
  - Start time represented by the telemetry data returned for the requested range.

* - `End`
  - End time represented by the telemetry data returned for the requested range.

* - `Duration`
  - Duration between the reported start and end times.

* - `Unique IPs`
  - Number of distinct source IP addresses.

* - `Unique Locations`
  - Number of distinct country/city locations derived from source IP addresses.

* - `Total Requests`
  - Total number of telemetry requests.

* - `Python Client Requests`
  - Requests identified as originating from a Python client.

* - `Web Client Requests`
  - Requests identified as originating from the web client.

* - `Unknown Client Requests`
  - Requests whose client value does not identify a known client.

* - `ICESat-2 Granules Processed`
  - Requests to the configured ICESat-2 processing endpoints.

* - `ICESat-2 Proxied Requests`
  - Requests to the configured ICESat-2 proxy endpoints.

* - `GEDI Granules Processed`
  - Requests to the configured GEDI processing endpoints.

* - `GEDI Proxied Requests`
  - Requests to the configured GEDI proxy endpoints.

```

In addition to the summary, the program prints detailed counts for source locations, clients, endpoints, telemetry request status codes, and alert status codes. It also prints a globe summary containing ICESat-2 and GEDI request totals.

# Additional Topics

### AWS Resources

The tool is configured to use the following AWS resources:

```{list-table}
:header-rows: 1

* - Resource
  - Configuration

* - AWS Region
  - `us-west-2`

* - Glue Database
  - `recorder-database`

* - Athena Workgroup
  - `recorder-workgroup`

* - Telemetry Table
  - `telemetry`

* - Alerts Table
  - `alerts`

* - S3 Bucket
  - `sliderule`

```

The tool uses Athena to execute the reporting queries, Glue to inspect and create missing table partitions, and S3 to determine which telemetry and alert partitions exist.

### Source Location

Source IP addresses are converted to `country, city` values using local MaxMind GeoLite2 databases. The configured databases are:

```text
/data/GeoLite2-Country.mmdb
/data/GeoLite2-City.mmdb
```

Localhost addresses are reported as `localhost, localhost`. If the IP address cannot be resolved, the location is reported as `unknown, unknown`.

### Grid Generation

Both generated maps use a global grid with a resolution of 0.25 degrees. The grid therefore contains 720 rows by 1440 columns.

The AOI grid is created directly from telemetry `aoi_x` and `aoi_y` values. The usage grid converts source locations to geographic coordinates using the local GeoNames databases before placing the request counts into the global grid.

Generated values are rendered using logarithmic normalization, with zero-valued cells made transparent.

### Partition Handling

Before querying Athena, the tool determines the requested date partitions and checks both S3 and Glue.

If an expected partition exists in S3 but is missing from the Glue table, the tool creates the corresponding Glue partition. This allows the reporting query to include telemetry or alert data that has already been written to S3 but has not yet been registered in Glue.

### Query Processing

Athena queries are polled until they succeed, fail, or reach the five-minute query timeout. Query results are retrieved using the Athena results paginator.

During execution, progress is displayed using `.` characters while the query is running and `!` characters while result pages are being retrieved.

### Output

The report is printed to standard output. Detailed sections are displayed for:

```text
Source Locations
Clients
Endpoints
Request Codes
Alert Codes
Summary
Globe
```

Source locations, clients, endpoints, and status-code sections are sorted by count in descending order. The summary and globe sections retain their defined field order.

### ICESat-2 and GEDI Counts

The summary groups endpoint counts into ICESat-2 and GEDI categories using the endpoint lists defined by the program.

The globe summary reports the total counts for:

```text
icesat2
gedi
```
