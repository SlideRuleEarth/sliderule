# Release v1.3.x

2022-01-20

Version description of the v1.3.3 release of ICESat-2 SlideRule.

## New Features

* **v1.3.3** - Logging improvements with Promtail/Loki/Grafana (labels, hostname information, formatting)

* **v1.3.3** - SlideRule now supports sending application metrics to Grafana

* **v1.3.0** - YAPC algorithm implemented as an option when using the atl06, atl06p, atl03s, and atl03sp APIs [#100](https://github.com/SlideRuleEarth/sliderule/issues/100)

* **v1.3.0** - Signal confidence selection can be specified as a list in addition to a single value. [#19](https://github.com/SlideRuleEarth/sliderule/issues/19)

* **v1.3.0** - There was a memory leak vulnerability in the previous versions of the code which was unlikely but possible.  It could occur when a dataset read timed out.  This vulnerability has been fixed, but the consequence is that the ATL08 processing is significantly slower.  Future versions of the code will attempt to bring the ATL08 processing back down.

## Major Issues Resolved

* **v1.3.1** - Security updates

* **v1.3.1** - Improved ATL08 performance [c3c635c](https://github.com/SlideRuleEarth/sliderule-icesat2/commit/c3c635cd88935a7783d3fa19654af0362e5ede0c)

* **v1.3.1** - Robust error handling added to earth data credential processing [3c9cac4](https://github.com/SlideRuleEarth/sliderule-project/commit/3c9cac4da3a6990ca0454f73014e8757ad518459)

* **v1.3.1** - Health check added to monitor for orchestrator [f3318f2](https://github.com/SlideRuleEarth/sliderule-project/commit/f3318f2fe68de7971d8ad2179cc76d9429de5f32)

* **v1.3.0** - Fixed bug in Python client where output was not being sorted correctly by time [ed4999d](https://github.com/SlideRuleEarth/sliderule-python/commit/ed4999dda4501cff1772ae7178e84a72e4249fb4)

## Minor Issues Resolved

* **v1.3.3** - Lua endpoints in separate directory from Lua extensions [#104](https://github.com/SlideRuleEarth/sliderule/issues/104)

* **v1.3.3** - Continued security rule updates [#4](https://github.com/SlideRuleEarth/sliderule-project/issues/4)

* **v1.3.3** - Fixed usage of icesat2.cmr in Grand Mesa demo [#68](https://github.com/SlideRuleEarth/sliderule-python/issues/68)

* **v1.3.3** - Fixed Python dependency issues in API Widgets demo [#59](https://github.com/SlideRuleEarth/sliderule-python/issues/59)

* **v1.3.3** - Parameterized pytests to support running them against a local server [1121213](https://github.com/SlideRuleEarth/sliderule-python/commit/11212130703b732c2ae72f15a4947bea075fd177)

* **v1.3.2** - Removed failing ECS workflow [#64](https://github.com/SlideRuleEarth/sliderule-python/issues/64)

* **v1.3.2** - Fixed failing unpinned tests [#65](https://github.com/SlideRuleEarth/sliderule-python/issues/65)

* **v1.3.1** - Added additional statistics and performance tests for H5Coro [perftest.py](https://github.com/SlideRuleEarth/sliderule-project/blob/main/tests/perftest.py)

* **v1.3.0** - Fixed minor issues related to compiling SlideRule with clang [3c80dbd](https://github.com/SlideRuleEarth/sliderule-python/commit/3c80dbd5068608094a2b5ad8af0245b2f39e3e87)

## Getting This Release

[https://github.com/SlideRuleEarth/sliderule/releases/tag/v1.3.3](https://github.com/SlideRuleEarth/sliderule/releases/tag/v1.3.3)

[https://github.com/SlideRuleEarth/sliderule-icesat2/releases/tag/v1.3.3](https://github.com/SlideRuleEarth/sliderule-icesat2/releases/tag/v1.3.3)

[https://github.com/SlideRuleEarth/sliderule-python/releases/tag/v1.3.3](https://github.com/SlideRuleEarth/sliderule-python/releases/tag/v1.3.3)
