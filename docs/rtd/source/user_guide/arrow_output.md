# Arrow Output

## Overview

By default, SlideRule returns all processing results in a native (i.e. custom to SlideRule) format as soon as they are generated.  Those results are streamed back to the client and used by the client to construct a (Geo)DataFrame that is presented to the user.  But sometimes it is desirable to have SlideRule build a (Geo)DataFrame on the server, and then stream that dataframe back to the client for easy reconstruction.  This could be because the dataframe is quite large and the environment the client is running in does not have the resources to build the dataframe.  Or it could be that the results need to be stored directly in an S3 bucket and having the dataframe already built expedites that process.

To support this functionality, SlideRule uses the Apache Arrow library to build dataframes in either Parquet, CSV, or Feather formats.  When using Parquet, the server also provides the option for using the **GeoParquet** convention to populate a geometry column and metadata compatible with **GeoPandas**.

## Parameters

To control writing the data to an Arrow supported format, the `output` parameter is used.

* `output`: settings to control how SlideRule outputs results
    * `path`: the full path and filename of the file to be constructed by the client, ***NOTE*** - the path MUST BE less than 128 characters
    * `format`: the format of the file constructed by the servers and sent to the client (currently, only GeoParquet is supported, specified as "parquet")
    * `open_on_complete`: boolean; if true then the client is to open the file as a DataFrame once it is finished receiving it and writing it out; if false then the client returns the name of the file that was written
    * `as_geo`: if the `parquet` format is specified, write the data compliant with the `GeoParquet` specification
    * `with_checksum`: include a checksum of the returned file in the response
    * `with_validation`: run the Apache Arrow validation routine on the resulting file before returning it to the user
    * `region`: AWS region when the output path is an S3 bucket (e.g. "us-west-2")
    * `asset`: the name of the SlideRule asset from which to get credentials for the optionally supplied S3 bucket specified in the output path
    * `credentials`: the AWS credentials for the optionally supplied S3 bucket specified in the output path
      - `aws_access_key_id`: AWS access key id
      - `aws_secret_access_key`: AWS secret access key
      - `aws_session_token`: AWS session token
    * `fields`: the list of fields to include in the file output, trimming anything not found in this list; this is only supported in ***x-series*** endpoints (e.g. atl03x, atl13x, atl24x, etc.)

```python
parms {
    "output": { "path": "grandmesa.parquet", "format": "parquet", "open_on_complete": True }
}
```

## S3 Output to User Bucket

SlideRule supports writing the output to an S3 bucket instead of streaming the output back to the client.  In order to enable this behavior, the `output.path` field must start with "s3://" followed by the bucket name and object key.  For example, if you wanted the result to be written to a file named "grandmesa.parquet" in your S3 bucket "mybucket", in the subfolder "maps", then the output.path would be "s3://mybucket/maps/grandmesa.parquet".  When writing to S3, it is required by the user to supply the necessary credentials.  Here is an example code snippet for writing to your own bucket.

```python
import os
import argparse
import configparser

# Setup Config Parser for Credentials
home_directory          = os.path.expanduser('~')
aws_credential_file     = os.path.join(home_directory, '.aws', 'credentials')
config                  = configparser.RawConfigParser()

# Read AWS Credentials
config.read(aws_credential_file)

# Populate Output Parameters
parms["output"] = {
    "path": "s3://mybucket/myfile.parquet",
    "format": "parquet",
    "open_on_complete": False,
    "region": "us-west-2",
    "credentials": {
        "aws_access_key_id": config.get('default', 'aws_access_key_id'),
        "aws_secret_access_key": config.get('default', 'aws_secret_access_key'),
        "aws_session_token": config.get('default', 'aws_session_token')
    }
}
```

## S3 Staging

SlideRule also supports writing the output to its own S3 bucket for times when temporary storage is needed and the user does not have access to a bucket they own.

To use this feature, the following parameters can be used:
```python
"output": {
    "asset": "sliderule-stage",
    "path": "myfile.parquet",
    "open_on_complete": False,
}
```

The `sliderule-stage` asset tells sliderule to stage the output in SlideRule's own bucket.  The full path of the file is then returned back to the user so that the user can later open the file directly.  For instance, in the above example, a call to `icesat2.atl06p` could return "s3://sliderule-public/myfile.parquet".  If no `path` is specified, the server code generates a random file name and uses it to store the results.  The generated file name including the path is returned back to the user.

There are a couple of constraints to using this feature:
* In order to access the file staged in the SlideRule owned bucket, the user has to have read-access to the bucket.  Typically, this is only provided through partnered organizations like [CryoCloud](https://cryointhecloud.com/).  When running on CryoCloud, a user can specify the "sliderule-stage" asset, and know that they returned file path is immediately accessible from their environment.
* Files are only stored for a short period of time before they are automatically deleted.  Typically, the retention time is 2 weeks.
