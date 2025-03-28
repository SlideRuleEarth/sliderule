# ATL24 Processing Run

2025-03-28

## Background

The University of Texas at Austin and Oregon State University partnered with the SlideRule team (University of Washington, Goddard Space Flight Center, and Wallops Flight Facility) to develop and generate a Near-Shore Coastal Bathymetry Product for ICESat-2 called ATL24.  The initial development and generation of the data product was kicked off in January of 2024, started in earnest in May of 2024, and completed April 1st, 2025.

ATL24 is a photon classification for ICESat-2 photons in ATL03.  Algorithms designed and implemented by UT and OSU were integrated into SlideRule and run as the `atl24g` service.  Each processing request to `atl24g` provided an ATL03 granule and produced a corresponding ATL24 granule. All ATL03 version 006 photons within a global bathymetry search mask that were within 50m above and 100m below the geoid were processed and labelled as either: unclassified, sea surface, or bathymetry.

## Statistics

* *452,173* ATL03 granules were processed (constituting cycles 1 through 25).
* *277,255* ATL24 granules were produced
* *145,283* processing runs resulted in empty output (no bathymetry was identified) and therefore no ATL24 granule was produced
* *29,635* processing runs failed to produce a valid result
* *27.649 TB* of ATL24 data was produced
* *989.46 B* photons were classified
* *59.19%* of classified photons were sea surface
* *0.73%* of classified photons were bathymetry

## Description

The ATL24 processing run started on February 11th, 2025 and completed February 24th, 2025.  The processing system consisted of 4 private SlideRule clusters containing 100 processing nodes each.  The 4 clusters were spread across the 4 availability zones (a, b, c, and d) in AWS us-west-2.  This maximized the availability of spot instances while maintaining clusters within a single availability zone for performance and cost efficiency.  The nodes in each cluster consisted of graviton3 instances with 32GB to 64GB of memory (r8g.2xlarge, r7g.2xlarge, m7g.2xlarge, c7g.4xlarge).

Four instantiations of a multithreaded Python script (`sdp_runner.py`) orchestrating the generation of ATL24 was executed on a dedicated r8g.2xlarge instance in us-west-2. Tmux was used to maintain an execution environment for each script over the course of the product run.  To iterate through all the ATL03 granules in a given ICESat-2 cycle, the Python script issued 100 processing requests at a time, and directed the output to an S3 staging bucket.

After all ATL03 granules were processed, a single instantiation of a multiprocess Python script (`bathy_collection.py`) executed on a dedicated r8g.2xlarge instance in us-west-2 and performed a minimal level of validation for each ATL24 granule.  A list of valid ATL24 granules was then produced and delivered to WFF.

WFF performed the transfer of ATL24 granules residing in the S3 bucket in us-west-2 to the NSIDC.  Granules were downloaded from the S3 bucket to a GSFC server, then transferred from there to the SIPS system at GSFC, and from there transferred to the NSIDC on-premises ingest system.  The NSIDC then handled the transfer of ATL24 granules to Earthdata Cloud S3 buckets residing in us-west-2.

## Lessons Learned

* During the first week of processing, it was difficult to get get access to the number of spot instances we wanted.  Requests of 100 instances often failed and there were long stretches of downtime when no processing was ocurring.  Two steps were taking to mitigate this.  (1) We increased our bid price for spot instances by 1 cent, which put us just above the current rate.  (2) Once we obtained spot instances, we found that AWS almost never took them away, so we held on to them even through breaks in our cycle processing.  (Previously, once a cycle was completed, the processing of the next cycle would have started over with a new spot instance request; but this proved much less efficient than reusing the same deployment for the next cycle).

* Once we started getting all of the spot instances we were requesting, we hit a quota within AWS for how much compute capacity we could deploy.  We requested an increase in that quota and within the day it was approved and we were able to proceed.  In the future, the quota should be increased pro-actively with margin prior to the start of the processing run.

* There were two validation checks in the processing code that caused no issues in our small test runs, but were overly restrictive when running globally.  (1) We required all ATL03 granules to have all 6 beams present; this caused valid ATL03 granules with only the strong beams present to be discarded.  (2) We had an internal check that the data within an HDF5 chunk in the ATL03 granule, when compressed, was smaller than or equal to in size to the uncompressed data.  In both cases, these checks were removed and the granules that had failed these checks were reprocessed.

* In order to accerlate the generation of ATL24, the VIIRS data used for turbidity calculations was staged in a private S3 bucket.  But at the time the data was staged, the later months of ATL03 had not been released; yet when we executed the processing run, those later ATL03 granules had been released.  The result was that the last couple cycles of ATL03 all failed because the VIIRS data was not found.  Once discovered, the necessary VIIRS data was staged and the ATL24 granules for the last couple of cycles was reprocessed.

* Throughout the processing run there were intermittent CMR failures.  As a result, the processing code had to be very conservative in retrying the CMR requests.  This often produced very long processing runs for granules that would otherwise have completed quickly.

## Future Work

* Currently, when we request a certain number of spot instances using terraform, if that number of spot instances is not supplied, terraform issues an error and we do not receive any spot instances.  We need to change our terraform so that if we request X number of spot instances, and only Y number of spot instances is available, then Y spot instances are returned.
