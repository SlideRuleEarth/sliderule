# Accessing Earthdata Cloud

2022-04-01

1. Setup .netrc file
```bash
$ echo 'machine urs.earthdata.nasa.gov login {uid} password {password}' >> ~/.netrc
$ chmod 0600 ~/.netrc
```
where {uid} and {password} are your Earthdata Login username and password.

2. Get AWS access credentials
```bash
$ curl -b ~/.urs_cookies -c ~/.urs_cookies -L -n https://data.nsidc.earthdatacloud.nasa.gov/s3credentials
```
