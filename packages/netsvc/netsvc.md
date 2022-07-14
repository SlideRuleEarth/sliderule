## Install cURL Library

To get the latest information and releases for cURL, please refer to their website: https://curl.se/libcurl/c/libcurl-tutorial.html.

For Ubuntu 20.04
```bash
$ sudo apt install curl libcurl4-openssl-dev
```

Note: the installation of libcurl must support SSL and TLS.  Use the `curl-config --feature` command to verify that these features are present.
If they are not present, follow the instructions in the libcurl-tutorial linked above to download, build, and install a local version that does support these features.