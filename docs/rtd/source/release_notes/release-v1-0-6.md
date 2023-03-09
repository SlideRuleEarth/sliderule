# Release v1.0.6

2021-06-08

Version description of the v1.0.6 release of ICESat-2 SlideRule

* Added support for NSIDC/Cumulus data in AWS US-West-2
  * New [netsvc](https://github.com/ICESat2-SlideRule/sliderule/tree/main/packages/netsvc) package that uses `libcurl` for making https requests.
  * New capability to "fork" a Lua script [2385b99](https://github.com/ICESat2-SlideRule/sliderule/commit/2385b99bb80bd20aabbf83b357b9fbf5d088f740)
  * Reworked internal handling of assets; an asset now defines an I/O driver which must be registered prior to an asset being requested.
  * New capability to fetch files from S3 directly from a Lua script using `aws.s3get`
  * Created CredentialStore module in `aws` package for managing AWS credentials: [CredentialStore.cpp](https://github.com/ICESat2-SlideRule/sliderule/blob/a86b719b429c75289234c00d968cd9946c8710de/packages/aws/CredentialStore.cpp)
  * Created authentication script (written in lua) that runs as a daemon and maintains up-to-date AWS credentials
  * Fixed and enhanced TimeLib to support AWS time formats

* Added support for metric collection along with a `metric` endpoint and associated Python client utility [query_metrics.py](https://github.com/ICESat2-SlideRule/sliderule-python/blob/50f64a9039630cb8390345c87110adab08ded79f/utils/query_metrics.py)

* Added support for tailing server logs remotely via the `tail` endpoint and associated Python client utility [tail_events.py](https://github.com/ICESat2-SlideRule/sliderule-python/blob/50f64a9039630cb8390345c87110adab08ded79f/utils/tail_events.py)

* Added native HttpClient module in `core` package: [HttpClient.cpp](https://github.com/ICESat2-SlideRule/sliderule/blob/a86b719b429c75289234c00d968cd9946c8710de/packages/core/HttpClient.cpp)

* Added support for building on CentOS 7 [1a728b9](https://github.com/ICESat2-SlideRule/sliderule/commit/1a728b919dca2d260f1166bcd73dec1a37821d7c)

## Known Issues

1. Consul-Exit is not working and therefore a node does not dissappear from the service group when it goes down.

## Getting this release

[https://github.com/ICESat2-SlideRule/sliderule/releases/tag/v1.0.6](https://github.com/ICESat2-SlideRule/sliderule/releases/tag/v1.0.6)
[https://github.com/ICESat2-SlideRule/sliderule-icesat2/releases/tag/v1.0.6](https://github.com/ICESat2-SlideRule/sliderule-icesat2/releases/tag/v1.0.6)
[https://github.com/ICESat2-SlideRule/sliderule-python/releases/tag/v1.0.6](https://github.com/ICESat2-SlideRule/sliderule-python/releases/tag/v1.0.6)

