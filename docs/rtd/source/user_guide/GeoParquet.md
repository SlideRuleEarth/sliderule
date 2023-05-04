# GeoParquet

2023-02-24

## Background

Apache Parquet is an open source, column-oriented data file format designed for efficient data storage and retrieval. It has strong support in Python/DataFrames and is a popular data format for big data analytics.  See https://parquet.apache.org/ for more details.

GeoParquet is a meta data and file organization covention overlayed on top of the Parquet format that provides a standard way of storing geospatial information inside Parquet files.  It has growing support in geospatial analysis tools, including Python/GeoDataFrames.  See https://geoparquet.org/ for more details.

## Overview

SlideRule currently supports returning results back to data users as GeoParquet files.  These files are built on the server and either streamed back directly to the user, or uploaded to a user-specified S3 bucket for later access. To specify the GeoParquet option, the request must include the `output` parameter with the `output.format` field set to **"parquet"**. See the section on [output parameters](./SlideRule.html#output-parameters) for more details.


### S3 as a destination

To specify S3 as a destination, the `output.path` field must start with **"s3://"**.  (For example, "s3://mybucket/maps/grandmesa.parquet").

The SlideRule project does not maintain any S3 buckets that are open for public access; it is therefore incumbent on the user to provide a path to an S3 bucket they have access to, and to also provide the temporary credentials in their request for SlideRule to use to write to the bucket.

Methods for obtaining temporary AWS credentials are outside the scope of this user guide, yet it is strongly encouraged that the credentials provided are as limited in scope as possible.  For the purposes of SlideRules, the only access that is necessary is "s3:PutObject".


### Constraints

* Currently, only support for the `atl06`, `atl08`, and flattened `atl03` records is provided.  This means that the ICESat-2 `compact` parameter being set is not supported when outputting to GeoParquet, and the `atl03` results may look slightly different between native runs and runs that request the GeoParquet format.

* The results in the GeoParquet file are not sorted.

* The SlideRule server side version information only includes the server core version information and does not include version information for any of the plugins that the server has loaded and is running.
