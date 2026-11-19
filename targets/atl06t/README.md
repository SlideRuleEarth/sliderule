# atl06t

The Tiled ATL06 product is a global restructuring of the along-track ATL06 standard data product into geospatially optimized tiles.

## Tiling

The Tiled ATL06 product uses Google's S2 discrete global gridding system (DGGS) to partition the Earth's sphere into a hierarchy of cells (tiles) at 31 levels of resolution.  At the lowest resolution (level 0) the sphere is divided into 6 cells corresponding to the sides of a circumscribing cube.  At each successive level, the cells are further divided into four child cells, such that by the time we get to level 30, each cell is just under a cm^2 in area.

The Tiled ATL06 product organizes the data into ***files that map to S2 level 7 cells***, with ***segments that are labelled with and sorted by their S2 level 18 cell***.

See the [S2 Geometry](https://s2geometry.io/devguide/s2cell_hierarchy) website for additional information.

## Naming

`s3://sliderule-products/atl06t/v4/tiles/ATL06T_S{CELL_ID:016X}.parquet`

where ***CELL_ID*** is the S2 Geometry level 7 cell ID written in ASCII hexadecimal uppercase notation.  These files contain the full mission timespan of data up to the current year of operation. Data collected by the mission for the current year is stored in a separate set of files named:

`s3://sliderule-products/atl06t/v4/tiles/ATL06T_CY_S{CELL_ID:016X}.parquet`

where ***CY*** stands for "Current Year".  These files are updated every 2 to 3 months as new data is released by the project.

## Format

Each tile is formatted as a **GeoParquet** file with the following characteristics:
* The level 18 cell ID column is used as the index.
* Each row group is targeted to be about 8MB in size.
* Row groups do not split cells (two rows with the same cell ID are guaranteed to be in the same row group).
* Only the `cell_id` and `time_ns` columns have footer statistics.
* A small set of high priority columns are ordered as the first column chunks stored in each row group; all other column chunks are non-deterministically ordered afterwards by the default settings of the Arrow library.
* String columns use dictionary encoding while all other columns use zStandard compression.

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

## Statistics

Sitting next to the data tiles are statistics tiles which can be used for more complex queries and data visualization.  These tiles are stored as **Parquet** files, and contain statistics for each column of the source Tiled ATL06 dataset.

The statistics tiles are named as follows:

`s3://sliderule-products/atl06t/v4/stats/ATL06S_S{CELL_ID:016X}.parquet`

where ***CELL_ID*** is the S2 Geometry cell ID for the level of the file.

The statistics tiles are organized as a pyramid of successive levels of resolution where each statistics tile contains aggregated statistics for the S2 cell level that is 7 levels above the file cell level.  The table below provides the full mapping of the statistics tiles available.

| File Level | Statistics Level |
|:----------:|:----------------:|
| 0 | 7 |
| 1 | 8 |
| 2 | 9 |
| 3 | 10 |
| 4 | 11 |
| 5 | 12 |
| 6 | 13 |
| 7 | 14 |

The data in the statistics tiles are as follows:

| Column | Description |
|:------:|:-----------:|
| cell id | cell level over which statistics are aggregated |
| lat_min | minimum latitude of an ATL06 segment in this cell |
| lat_max | maximum latitude of an ATL06 segment in this cell |
| lon_min | minimum longitude of an ATL06 segment in this cell |
| lon_max | maximum longitude of an ATL06 segment in this cell |
| row_count | number of ATL06 segments in this cell |
| row_group_start | row group number of first row group in source tile containing an ATL06 segment in this cell |
| row_group_end | row group number of last row group in source tile containing an ATL06 segment in this cell |
| {column}_min | minimum value for the column in this cell |
| {column}_max | maximum value for the column in this cell |
| {column}_mean | mean value for the column in this cell |

## Background

* S2 operations are supported by community and commercial libraries with native support for C++, Python, JAVA, and DuckDB.

* The S2 cell geometry has strong guarantees that make working with it easy for developers and users.  For example, a parent cell contains exactly the set of data contained by the union of its child cells, and data contained within a parent cell is contained only within a single child cell.

* S2 cells use a Hilbert Curve ordering that correlates serial storage locality with spatial locality; this allows the data within the file to be organized in a way that minimizes the number of reads needed to retrieve a spatial area.

* The deterministic naming of files precludes the need for a separate catalog or metadata service for spatial subsetting.  An S2 library can quickly and locally generate a list of level 7 cells that cover an area of interest, from which the list of ATL06T filenames can be derived.

* Files that do not split across time favor workflows that start with a spatial query since they organize all of the data within a spatial region as close as possible in memory.  Temporal filters for reasonably sized spatial regions are still fast, but requests that do not have a spatial component will suffer from having to open up many tiles.

* Keeping a separate set of files for the current year allows data to be added to the collection without reprocessing the entire dataset each time.  Once a year the entire dataset can be regenerated and optimized for spatial queries.

* GeoParquet is a columnar format optimized for analytical workflows with good support for S3 object storage access (Pandas, DuckDB, Spark, Dask, etc.).

* The column chunks of the most used columns are stored first in the row group to minimize gaps in the range requests.  Arrow will merge ranges that are separated by some minimal gap; which is why requesting columns that are stored next to each other reduces the overread caused by merged ranges, while also minimizing the additional network requests caused by unmerged gaps.

* The footer statistics are limited to only those columns used in the majority of queries in order to reduce the size of the footer.  Since every access to a parquet file must read the entire footer, the cost of this read is experienced by every query regardless if every part of the footer is used by the query.

* The current generation of the tiled ATL06 product targets a maximum footer size of 25MB with a maximum of 5000 row groups.  These maximums are present in the dense polar regions.  At mid-latitudes some tiles have only a single row group.
