---
title: "&#128640; Private Cluster Overhaul"
excerpt: "1/21/26: The provisioning system at ps.slideruleearth.io has been replaced by more streamlined services at provisioner.slideruleearth.io and login.slideruleearth.io.  See release notes for full details."
categories: updates
---

The public cluster has been updated to release v5.0.3.

### Full release notes

[https://slideruleearth.io/web/rtd/developer_guide/release_notes/release-v05-00-02.html](https://slideruleearth.io/web/rtd/developer_guide/release_notes/release-v05-00-02.html)

### TL;DR

* There are breaking changes in how private clusters are deployed and accessed. See the [private clusters guide](https://slideruleearth.io/web/rtd/developer_guide/articles/private_clusters.html) for instructions on how to work with the new system.
* The following x-series endpoints have been added: `atl06x`, `atl08x`, `gedi01bx`, `gedi02ax`, `gedi04ax`
* The use of older p-series endpoints is discouraged; please update your code to use the analogous x-series endpoint.  Reach out via our [contact us](https://slideruleearth.io/web/contact/) page if you need assistance.