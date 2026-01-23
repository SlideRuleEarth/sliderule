# Release v5.0.5

2025-01-23

Version description of the v5.0.5 release of SlideRule Earth.

## Known Issues

* The GitHub actions related to running the pytests and the selftest are currently not working.  This is due to them being transitioned to the provisioning system.

## Issues Resolved

* [1fd0f03](https://github.com/SlideRuleEarth/sliderule/commit/1fd0f03306877e11fbc0f2c395b039d8fcfb6c5d) - fixed defaults for `atl06x` and `atl08x`

* [e2c7cc8](https://github.com/SlideRuleEarth/sliderule/commit/e2c7cc8b379f21ea92b2f17d55b79fd4306e51bc) - project bucket passed through to cluster (fixed issue of netrc file not being found)

* [0b1f32c](https://github.com/SlideRuleEarth/sliderule/commit/0b1f32c40ea1ce7f7867aca852b81d477d6ce6b9) - provisioner check for org membership fixed (in addition to any checks related to a JWT claim that is a json array)

* [e8011fc](https://github.com/SlideRuleEarth/sliderule/commit/e8011fc22639addf2cddb2ecf89b72246aac0e34) - GitHub PAT key permissions reduced to minimal set

* [e65e01f](https://github.com/SlideRuleEarth/sliderule/commit/e65e01fc004611c5445b04fdfbdb2eccea9eceeb) - "org role" header passed to requests to be used by `ace` and `execre` as opposed to using "is public"

* [3a77550](https://github.com/SlideRuleEarth/sliderule/commit/3a77550f62af6535a815e10ad3762b8a4c23966b) - added ability to select non-nan values when pulling only a single value from a raster

* Cluster has the correct permissions to allow firehose posts

## Getting This Release

[https://github.com/SlideRuleEarth/sliderule/releases/tag/v5.0.5](https://github.com/SlideRuleEarth/sliderule/releases/tag/v5.0.5)

## Benchmarks
> clients/python/utils/benchmark.py
```
```

## Baseline
> clients/python/utils/baseline.py
```
```