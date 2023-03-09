# Release v1.5.x

2023-01-11

Version description of the v1.5.x set of releases of ICESat-2 SlideRule.  This series of releases represent a set of beta builds for the upcoming v2.0.0 release and often include incomplete and unstable functionality.  The last stable release prior to the v1.5.x series is v1.4.6.  The next stable release will be v2.0.0, which will not be backward compatible with the v1.4.6 release and will require users to update their Python client.

## New Features

The SlideRule system underwent a gradual architectural shift from a single public cluster, to a set of provisioned clusters that can be either public or private.  Supporting private clusters entail authentication, encrypted http traffic (i.e. https), and a provisioning system that deploys/destroys and monitors each cluster.

## Issues Resolved

### Cluster

- Replace AWS S3 client in H5Coro with local HttpClient [#52](https://github.com/ICESat2-SlideRule/sliderule/issues/52)

- Support http connection keep-alive [#58](https://github.com/ICESat2-SlideRule/sliderule/issues/58)

- Implement dynamic result records [#53](https://github.com/ICESat2-SlideRule/sliderule/issues/53)

- Add direct http access option for h5coro [#89](https://github.com/ICESat2-SlideRule/sliderule/issues/89)

- Use aws_s3_get for s3cache instead of aws transfer library [#80](https://github.com/ICESat2-SlideRule/sliderule/issues/80)

- Generic ATL03/06/08 subsetter to support cross-referencing calculated fields [#120](https://github.com/ICESat2-SlideRule/sliderule/issues/120)

- Networking issues on one of the deployments [#6](https://github.com/ICESat2-SlideRule/sliderule-build-and-deploy/issues/6)

- Node restarted for no discernable reason [#8](https://github.com/ICESat2-SlideRule/sliderule-build-and-deploy/issues/8)

- Application Load Balancer adds two seconds of latency [#2](https://github.com/ICESat2-SlideRule/sliderule-build-and-deploy/issues/2)

- Implement automated memory test suite [#62](https://github.com/ICESat2-SlideRule/sliderule/issues/62)

- Clean up lua object calls to return self on success [#114](https://github.com/ICESat2-SlideRule/sliderule/issues/114)

- Locking Lua objects no longer in use [#113](https://github.com/ICESat2-SlideRule/sliderule/issues/113)

- LuaObject "name" method should return reference to self [#55](https://github.com/ICESat2-SlideRule/sliderule/issues/55)

- Memory issues due to requests not completing [#124](https://github.com/ICESat2-SlideRule/sliderule/issues/124)

- Decouple H5 related self tests from large ATL03 data file [#14](https://github.com/ICESat2-SlideRule/sliderule/issues/14)

- cURL command https://{{ORG}.testsliderule.org/source/version returns '%' at end of response [#134](https://github.com/ICESat2-SlideRule/sliderule/issues/134)

- Race condition in HttpServer request handling [#139](https://github.com/ICESat2-SlideRule/sliderule/issues/139)

- Enable and configure autoscalling for demo container service [#20](https://github.com/ICESat2-SlideRule/sliderule-build-and-deploy/issues/20)

- Move base64 functions to StringLib [#141](https://github.com/ICESat2-SlideRule/sliderule/issues/141)

- Need way to pass through timeout to S3CurlIODriver [#143](https://github.com/ICESat2-SlideRule/sliderule/issues/143)

- Orchestrator running in HAProxy has intermittent runtime errors [#144](https://github.com/ICESat2-SlideRule/sliderule/issues/144)

- Possible memory leak in GeoJsonRaster [#149](https://github.com/ICESat2-SlideRule/sliderule/issues/149)

- RqstParms to support radius and sampling algorithm [#166](https://github.com/ICESat2-SlideRule/sliderule/issues/166)

- RasterSampler to support zonal statistics [#167](https://github.com/ICESat2-SlideRule/sliderule/issues/167)

### Python Client

- Implement raster support for northern and southern projections [#88](https://github.com/ICESat2-SlideRule/sliderule-python/issues/88)

- Implement k-means clustering for large requests [#87](https://github.com/ICESat2-SlideRule/sliderule-python/issues/87)

- Voila leaks memory [#96](https://github.com/ICESat2-SlideRule/sliderule-python/issues/96)

- Max retries exceeded with url: Caused by ConnectTimeoutError [#102](https://github.com/ICESat2-SlideRule/sliderule-python/issues/102)

- problem with poly=None [#105](https://github.com/ICESat2-SlideRule/sliderule-python/issues/105)

- Catch exceptions in Voila demo [#101](https://github.com/ICESat2-SlideRule/sliderule-python/issues/101)

- SlideRule identifying 0 resources to process [#107](https://github.com/ICESat2-SlideRule/sliderule-python/issues/107)

- ResourceWarning: unclosed socket error [#106](https://github.com/ICESat2-SlideRule/sliderule-python/issues/106)

- Feature request: export ref_azimuth and ref_elev with ATL03 [#103](https://github.com/ICESat2-SlideRule/sliderule-python/issues/103)

### Provisioning System

- The Orchestrator failed to respond to requests [#7](https://github.com/ICESat2-SlideRule/sliderule-build-and-deploy/issues/7)

- First view of org didn't show correct state [#2](https://github.com/ICESat2-SlideRule/sliderule-ps-web/issues/2)

- Account forecasts go below zero [#3](https://github.com/ICESat2-SlideRule/sliderule-ps-web/issues/3)

- Cluster IP link redirects to port 3000 [#5](https://github.com/ICESat2-SlideRule/sliderule-ps-web/issues/5)

- Organization pages should always be shown to staff [#6](https://github.com/ICESat2-SlideRule/sliderule-ps-web/issues/6)

- Docker containers should not need the git repository in order to run [#8](https://github.com/ICESat2-SlideRule/sliderule-ps-web/issues/8)

- Readme documentation updates [#9](https://github.com/ICESat2-SlideRule/sliderule-ps-web/issues/9)

- Account history plots switch between previous settings [#10](https://github.com/ICESat2-SlideRule/sliderule-ps-web/issues/10)

- The account plots require the page to be scrolled [#11](https://github.com/ICESat2-SlideRule/sliderule-ps-web/issues/11)

- The account history plot skips over days with no charges [#12](https://github.com/ICESat2-SlideRule/sliderule-ps-web/issues/12)

- Dynamically determine AWS elements to be costed [#1](https://github.com/ICESat2-SlideRule/sliderule-ps-server/issues/1)

- Make error msgs for invalid password on user signup useful [#104](https://github.com/ICESat2-SlideRule/sliderule-ps-web/issues/104)

- Sometimes the Destroy cluster is followed by an unwanted Update that restarts the cluster [#130](https://github.com/ICESat2-SlideRule/sliderule-ps-web/issues/103)

- Reloading page breaks state information shown [#4](https://github.com/ICESat2-SlideRule/sliderule-ps-web/issues/4)

- Inconsistent cluster state displayed [#32](https://github.com/ICESat2-SlideRule/sliderule-ps-web/issues/32)

- Reskin ps website [#49](https://github.com/ICESat2-SlideRule/sliderule-ps-web/issues/49)

- Open source this repository [#13](https://github.com/ICESat2-SlideRule/sliderule-ps-server/issues/13)

- pen source this repository [#51](https://github.com/ICESat2-SlideRule/sliderule-ps-web/issues/51)

- Add API for increasing number of nodes in cluster [#52](https://github.com/ICESat2-SlideRule/sliderule-ps-web/issues/52)

- Query version displays previous version on first refresh [#54](https://github.com/ICESat2-SlideRule/sliderule-ps-web/issues/54)

- Investigate the possibility of replacing lock by org with global redis queue by org [#70](https://github.com/ICESat2-SlideRule/sliderule-ps-web/issues/70)

- When submitting a PS_CMD from the org_manage_cluster page clear the terraform log and messages [#62](https://github.com/ICESat2-SlideRule/sliderule-ps-web/issues/62)

- When processing a PS_CMD if terraform fails validation step terraform fails all subsequent terraform cmds [#21](https://github.com/ICESat2-SlideRule/sliderule-ps-server/issues/21)

- Support deployment of provisioning system from scratch [#83](https://github.com/ICESat2-SlideRule/sliderule-ps-web/issues/83)

- Boxes should be spaced out even when expanded [#89](https://github.com/ICESat2-SlideRule/sliderule-ps-web/issues/89)

- Use tabs for cluster management page [#90](https://github.com/ICESat2-SlideRule/sliderule-ps-web/issues/90)

- The organization being viewed should be more prominent [#91](https://github.com/ICESat2-SlideRule/sliderule-ps-web/issues/91)

- Change status message color to green when deployed [#108](https://github.com/ICESat2-SlideRule/sliderule-ps-web/issues/108)

- Default account pages to monthly view [#135](https://github.com/ICESat2-SlideRule/sliderule-ps-web/issues/135)

- Option to force update a cluster [#136](https://github.com/ICESat2-SlideRule/sliderule-ps-web/issues/136)

### Documentation

- Links broken in API reference [#1](https://github.com/ICESat2-SlideRule/sliderule-docs/issues/1)

- The "make html" command generates warnings [#6](https://github.com/ICESat2-SlideRule/sliderule-docs/issues/6)

- Update "Under-The-Hood" documentation to reflect new architecture [#11](https://github.com/ICESat2-SlideRule/sliderule-docs/issues/11)

## Getting This Release

[https://github.com/ICESat2-SlideRule/sliderule/releases/tag/v1.5.10](https://github.com/ICESat2-SlideRule/sliderule/releases/tag/v1.5.10)

[https://github.com/ICESat2-SlideRule/sliderule-icesat2/releases/tag/v1.5.10](https://github.com/ICESat2-SlideRule/sliderule-icesat2/releases/tag/v1.5.10)

[https://github.com/ICESat2-SlideRule/sliderule-python/releases/tag/v1.5.10](https://github.com/ICESat2-SlideRule/sliderule-python/releases/tag/v1.5.10)

