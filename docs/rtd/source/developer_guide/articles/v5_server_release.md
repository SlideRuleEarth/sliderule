# 2025-12-08: Public Cluster Release v5

:::{note}
Version 5.0 of SlideRule has been officially released.  The changes include an overhaul of the private clusters, consistent ATL13 query formats, ATL24 release 002, improved earthdata error handling, and `h5p` slice support.  See release notes for full details.
:::

## Full release notes

[https://docs.slideruleearth.io/developer_guide/release_notes/release-v05-00-00.html](https://docs.slideruleearth.io/developer_guide/release_notes/release-v05-00-00.html)

## TL;DR

* ps.slideruleearth.io has been retired and replaced by provisioner.slideruleearth.io
* There are breaking changes (which will hopefully be minimal because they involved features that have been deprecated for some time)
* ATL24 release 002 is now the default
* The internal Asset Metadata Service is used for ATL24, ATL13, and 3DEP (only when specified)
* Earthdata error reporting was made more intuitive
* `h5p` supports slices