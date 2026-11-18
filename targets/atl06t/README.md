# atl06t

The Tiled ATL06 product is a global restructuring of the along-track ATL06 standard data product into geospatially optimized tiles of ATL06 segments contained within the tile's spatial boundaries.

## Tiling

The Tiled ATL06 product uses Google's S2 discrete global gridding system (DGGS) to partition the Earth's sphere into a hierarchy of cells (tiles) at 31 levels of resolution.  At the lowest resolution (level 0) the sphere is divided up into 6 cells corresponding to the sides of a circumscribing cube.  At each successive level, the cells are further divided into four child cells, such that by the time we get to level 30, each cell is just under a cm^2 in area.

The Tiled ATL06 product organizes the data into ***files that map to S2 level 7 cells***, with ***segments that are labelled with and sorted by their S2 level 18 cell***.

See [S2 Geometry](https://s2geometry.io/devguide/s2cell_hierarchy) website for additional information.

> **Background**
>
> -> S2 operations are supported by strong community and commercial libraries with native support for C++, Python, JAVA, and DuckDB.
>
> -> The S2 cell geometry has strong guarantees that make working with it easy for developers and users.  For example, A parent cell contains exactly the set of data contained by the union of its child cells; and data contained within a parent cell is contained only within a single child cell.
>
> -> S2 cells use a Hilbert Curve ordering that correlates serial storage locality with spatial locality; this allows the data within the file to be organized in a way that minimizes the number of reads needed to retrieve a spatial area.

## Naming

`s3://sliderule-products/atl06t/v4/ATL06T_S{CELL_ID:016X}.parquet`

where ***CELL_ID*** is the S2 Geometry level 7 cell ID written in ASCII hexadecimal uppercase notation.  These files contain the full mission timespan of data up to the current year of operation. Data collected by the mission for the current year is stored in a set of separate files named:

`s3://sliderule-products/atl06t/v4/ATL06T_CY_S{CELL_ID:016X}.parquet`

where ***CY*** stands for "Current Year".  These files are updated every 2 to 3 months as new data is released by the project.

> **Background**
>
> -> The deterministic naming of files precludes the need for a separate catalog or metadata service for spatial subsetting.  An S2 library can quickly and locally generate a list of covering level 7 cells for any area of interest from which the list of ATL06T filenames can be derived.
>
> -> Files that do not split across time favor workflows that start with a spatial query since they organize all of the data within a spatial region as close as possible in memory.  Temporal filters for reasonably sized spatial regions are still fast, but requests that do not have a spatial component will suffer needing to open up many tiles.
>
> -> Keeping a separate set of files for the current year allows data to be added to the collection without reprocessing the entire dataset each time.  Once a year the entire dataset can be regenerated and optimized for spatial queries.

## Format

Each tile is formatted as a ***GeoParquet** file with the following characteristics:
* The level 18 cell ID column is used as the index.
* Each row group strives to be about 8MB in size.
* Row groups do not split cells (two rows with the same cell ID are guaranteed to be in the same row group).
* Only the `cell_id` and and `time_ns` columns have footer statistics.
* A small set of high priority columns are ordered as the first column chunks stored in each row group; all other columns are non-deterministically ordered afterwards.
* String columns use dictionary encoding while all other columns use zStandard compression.


> **Background**
>
> -> GeoParquet is a columnar format optimized for analytical workflows with good support for S3 object storage access (Pandas, DuckDB, Spark, Dask, etc.).
>
> -> The column chunks of the most used columns are stored first in the row group to minimize gaps in the range requests.  Arrow will merge ranges that are separated by some minimal gap; which is why requesting columns that are stored next to each other reduces the overread caused by merged ranges, while also minimizing the additional network requests caused by unmerged gaps.
>
> -> The footer statistics are limited to only those columns used in the majority of queries.  Since every access to a parquet file must read the entire footer, the cost of this read is experienced by every query regardless if every part of the footer is used by the query.  By minimizing the size of the footer the average query is performed more efficiently.
>
> -> The current generation of the tiled ATL06 product targets a maximum footer size of 25MB with a maximum of 5000 row groups.  These maximums are present in the dense polar regions.  At mid-latitudes some tiles have only a single row group.

## Data

| Column | Units | Source | Description | High Priority | Footer Statistics |
|:------:|:-----:|:------:|:-----------:|:-------------:|:-----------------:|
| cell id | uint64 | derived | S2 geometry cell id | Yes | Yes |
| time_ns | Unix UTC | delta_time | | Yes | Yes |
| beam | string | derived | gt1l, gt1r, gt2l, gt2r, gtl3, gt3r | No | No |
| spot | int | derived | 1, 2, 3, 4, 5, 6 | No | No |
| rgt | int | derived | reference ground track | No | No |
| cycle | int | derived | ~91 day orbital cycle | No | No |
| region | int | derived | ICESat-2 global region, 1 to 14 | No | No |
| atl06_quality_summary | int | - | | Yes | No |
| delta_time | | - | | Yes | No |
| fpb_warning_flag | | - | | No | No |
| h_li | | - | | Yes | No |
| h_li_sigma | | - | | Yes | No |
| geometry.y | | latitude | | Yes | No |
| geometry.x | | longitude | | Yes | No |
| segment_id | | - | | Yes | No |
| sigma_geo_h | | - | | No | No |
| fpb_mean_corr | | bias_correction | | No | No |
| fpb_mean_corr_sigma | | bias_correction | | No | No |
| fpb_med_corr | | bias_correction | | No | No |
| fpb_med_corr_sigma | | bias_correction | | No | No |
| fpb_n_corr | | bias_correction | | No | No |
| med_r_fit | | bias_correction | | No | No |
| tx_mean_corr | | bias_correction | | No | No |
| tx_med_corr | | bias_correction | | No | No |
| dem_flag | | dem | | No | No |
| dem_h | | dem | | No | No |
| geoid_free2mean | | dem | | No | No |
| geoid_h | | dem | | No | No |
| dh_fit_dx | | fit_statistics | | Yes | No |
| dh_fit_dx_sigma | | fit_statistics | | No | No |
| dh_fit_dy | | fit_statistics | | Yes | No |
| h_expected_rms | | fit_statistics | | No | No |
| h_mean | | fit_statistics | | No | No |
| h_rms_misfit | | fit_statistics | | Yes | No |
| h_robust_sprd | | fit_statistics | | Yes | No |
| n_fit_photons | | fit_statistics | | Yes | No |
| n_seg_pulses | | fit_statistics | | No | No |
| sigma_h_mean | | fit_statistics | | No | No |
| signal_selection_source | | fit_statistics | | No | No |
| signal_selection_source_status | | fit_statistics | | No | No |
| snr | | fit_statistics | | No | No |
| snr_significance | | fit_statistics | | No | No |
| w_surface_window_final | | fit_statistics | | No | No |
| bckgrd | | geophysical | | No | No |
| bsnow_conf | | geophysical | | No | No |
| bsnow_h | | geophysical | | No | No |
| bsnow_od | | geophysical | | No | No |
| cloud_flg_asr | | geophysical | | No | No |
| cloud_flg_atm | | geophysical | | No | No |
| dac | | geophysical | | No | No |
| e_bckgrd | | geophysical | | No | No |
| layer_flag | | geophysical | | No | No |
| msw_flag | | geophysical | | Yes | No |
| neutat_delay_total | | geophysical | | No | No |
| r_eff | | geophysical | | No | No |
| solar_azimuth | | geophysical | | No | No |
| solar_elevation | | geophysical | | No | No |
| tide_earth | | geophysical | | No | No |
| tide_earth_free2mean | | geophysical | | No | No |
| tide_equilibrium | | geophysical | | No | No |
| tide_load | | geophysical | | No | No |
| tide_ocean | | geophysical | | No | No |
| tide_pole | | geophysical | | No | No |
| ref_azimuth | | ground_track | | No | No |
| ref_coelv | | ground_track | | No | No |
| seg_azimuth | | ground_track | | No | No |
| sigma_geo_at | | ground_track | | No | No |
| sigma_geo_r | | ground_track | | No | No |
| sigma_geo_xt | | ground_track | | No | No |
| x_atc | | ground_track | | Yes | No |
| y_atc | | ground_track | | Yes | No |
