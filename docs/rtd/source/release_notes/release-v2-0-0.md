# Release v2.0.0

2023-01-11

Version description of the v2.0.0 release of ICESat-2 SlideRule.

## New Features

Version 2.0.0 of SlideRule represents a major change to the SlideRule architecture and is NOT backward compatible with any of the previous releases.  The following is a list of changes in this major release.

* __New Domain__: SlideRule has moved from `http://icesat2sliderule.org` to `https://slideruleearth.io`.  This change was made to reflect the new scope of SlideRule which includes datasets (e.g. ArcticDEM and LandSat) beyond the ICESat-2 mission, and was necessary due to enhanced security measures and support for authenticated access.  The `icesat2sliderule.org` domain will continue to run v1.4.6 until all existing users have transitioned over to the new domain, at which point we will make `icesat2sliderule.org` an alias for `slideruleearth.io`.  The following are a list of the subdomains associated with our new domain:
    - `slideruleearth.io` and `www.slideruleearth.io`: the static website for the SlideRule project (documentation, links, tutorials, etc)
    - `ps.slideruleearth.io`: the SlideRule Provisioning system
    - `demo.slideruleearth.io`: the graphical demo of SlideRule functionality
    - `sliderule.slideruleearth.io`: the public SlideRule cluster
    - `{organization}.slideruleearthio.io`: the private SlideRule cluster associated with {organization}

* __Versioning__: with the v2.0.0 release, the SlideRule project will adhere to strict versioning guidelines which are checked during client initialization in the `icesat2.init` function.  The version string `vX.Y.Z` indicates the following:
    - `X`: major version, indicating compatibility; all components of SlideRule that share the same major version are compatible (e.g. the demo, the client, the provisioning system, and the cluster)
    - `Y`: minor version, indicating new features have been added; features are only guaranteed to work in the lowest common minor version number of the SlideRule components in use
    - `Z`: revision number, indicating a bug fix or security patch; this can be ignored by end users

* __Provisioning System__: SlideRule now includes a full-featured provisioning system for deploying, scaling, and cost tracking public and private clusters.  The public cluster will match the user experience provided at the old domain `icesat2sliderule.org` and will continue to provide a frictionless service for generating on-demand science products.  For organizations which want guaranteed availability and large scale processing resources, the Provisioning System provides a way to deploy private clusters accessible only by authenticated users that can scale up and down to match the budget of the owning organization.

* __ArcticDEM__: SlideRule now supports sampling the ArcticDEM datasets (both mosaic and strips) at each location an ATL06-SR elevation is produced, and providing those sampled values in the returned GeoDataFrame back to the user.  Multiple sampling algorithms are supported, as well as calculating zonal statistics.  New raster datasets are forth-coming.

* __Ancillary Fields__: Additional ATL03 subgroup datasets can now be included in the results of the calls to `atl03sp` and `atl06p`.  When requested, they appear as additional columns in the GeoDataFrame.  This feature was added to support bathymetry and other science use cases that used additional data provided in the ATL03 data that isn't provided as a part of the standard ATL06-SR processing.

* __GeoParquet__: SlideRule can now return results as a GeoParquet file which is built entirely on the servers and streamed directly back to the client.

* __Intelligent Load Balancing__: The underlying architecture of the cluster of processing nodes was reworked so that each processing node is allocated a fixed number of computational credits.  When a request is made to a cluster, the processing is routed to the nodes with the most available credits.  If there are no nodes with available credits, the request is held while the system polls for available nodes until a user-supplied timeout is reached, at which point the client is informed that the request could not be processed.


## Migration Guide

The following actions are necessary to start using the latest version of SlideRule.

1. Update your Python client via `conda update sliderule` or see these [installation instructions](https://slideruleearth.io/rtd/getting_started/Install.html) for a detailed discussion of how to install the client.

2. Change all of your Python scripts to point to the new domain: `icesat2.init("icesat2sliderule.org")` ==> `icesat2.init("slideruleearth.io")`.

For private clusters:

3. Create an account on the SlideRule Provisioning System `ps.slideruleearth.io`.

4. If you are already associated with an organization that has a private cluster on slideruleearth.io, then request membership to your organization using the Provisioning System.  If you want to create your own organization, please reach out to the SlideRule science team via one of the methods described on our [Contact Us](https://slideruleearth.io/contact/) page.

5. Set up a __.netrc__ file in your home directory with the following entry:
```
machine ps.slideruleearth.io login {your_username} password {your_password}
```

6. Modify your Python scripts to supply your organization in the call to initialize the client, like so:
```Python
icesat2.init("slideruleearth.io", organization="{your_organization}")
```

7. Depending on how your cluster is configured, a subsequent call to `sliderule.update_available_servers` may be needed to scale the cluster to the desired size.
```Python
sliderule.update_available_servers(num_desired, time_to_live)
available_servers = 0
while available_servers < num_desired:
    available_servers,_ = sliderule.update_available_servers()
```

## Getting This Release

[https://github.com/ICESat2-SlideRule/sliderule/releases/tag/v2.0.0](https://github.com/ICESat2-SlideRule/sliderule/releases/tag/v2.0.0)

[https://github.com/ICESat2-SlideRule/sliderule-icesat2/releases/tag/v2.0.0](https://github.com/ICESat2-SlideRule/sliderule-icesat2/releases/tag/v2.0.0)

[https://github.com/ICESat2-SlideRule/sliderule-python/releases/tag/v2.0.0](https://github.com/ICESat2-SlideRule/sliderule-python/releases/tag/v2.0.0)

