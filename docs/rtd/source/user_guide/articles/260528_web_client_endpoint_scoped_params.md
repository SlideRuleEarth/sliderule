# 2026-05-28: Web Client v4.5.0 - Endpoint-Scoped Advanced Options

:::{note}
Release v4.5.0 of the SlideRule web client hides Advanced Options that the selected endpoint ignores, so you no longer see controls that the server would silently discard.
:::

## Overview

The Advanced Options panel now adapts to the endpoint you've selected. The X-series endpoints (`atl06x`, `atl08x`, `atl24x`, `atl13x`) read pre-computed segments directly from HDF5 products, so the server discards photon-processing parameters for these endpoints. The web client now mirrors that behavior in the UI, eliminating misleading controls and preventing stale store state from leaking into the request.

## Panel visibility by endpoint

| Endpoint | Photon Selection | Extents | Surface Elevation | PhoREAL |
|---|---|---|---|---|
| `atl03x` variants, `atl06p`, `atl06sp`, `atl08p`, `atl03vp` | editable | editable | if applicable | if applicable |
| `atl06x` | hidden | **read-only: 40m / 20m** | hidden | n/a |
| `atl08x` | hidden | **read-only: 100m / 100m** | n/a | hidden |
| `atl24x`, `atl13x` | hidden | hidden | n/a | n/a |

For `atl06x` and `atl08x`, the Extents panel now displays the real segment dimensions baked into the data product as a disabled read-only field with a lock icon and tooltip.

## Request builder

The request builder gates photon-processing parameters (`fit`, `len`, `res`, `cnf`, `srt`, `yapc`, `atl08_class`, `pass_invalid`, `ats`, `cnt`, `dist_in_seg`) behind an `isPhotonAPI` check, so switching from a P-series to an X-series endpoint no longer carries stale parameters into the outgoing request.

## Pull request

[https://github.com/SlideRuleEarth/sliderule-web-client/pull/1075](https://github.com/SlideRuleEarth/sliderule-web-client/pull/1075)
