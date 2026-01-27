local runner = require("test_executive")
local earthdata = require("earth_data_query")

-- Requirements --

if (not sys.getcfg("in_cloud") and not runner.isglobal()) then
    return runner.skip()
end

-- Setup --

runner.authenticate({'nsidc-cloud'})
local asset_name = "icesat2"

-- Helper Functions --

local function check_expected(exp, df, index, t)
    for key,value in pairs(exp) do
        if index then
            runner.assert(math.abs(df[key][index] - value) <= t, string.format("%s[%d] => %f", key, index, df[key][index]))
        else
            runner.assert(math.abs(df:meta(key) - value) <= t, string.format("%s => %f", key, df:meta(key)))
        end
    end
end

local function check_columns(df, cols)
    for _,name in ipairs(cols) do
        runner.assert(df[name] ~= nil, string.format("column missing: %s", name))
    end
end

-- Self Test --
runner.unittest("ATL08 DataFrame", function()

    local parms = icesat2.parms({
        beams = "gt3r",
        resource = "ATL08_20200307004141_10890603_007_01.h5"
    })

    local atl08h5 = h5.object(asset_name, parms["resource"])
    local atl08df = icesat2.atl08x("gt3r", parms, atl08h5, core.EVENTQ)

    runner.assert(atl08df:waiton(60000), "timed out creating dataframe", true)
    runner.assert(atl08df:inerror() == false, "dataframe encountered error")

    -- Expected shape for ATL08_20200307004141_10890603_007_01.h5 / gt3r
    runner.assert(atl08df:numrows() == 10214, string.format("incorrect number of rows: %d", atl08df:numrows()))

    if atl08df:numrows() >= 1 then
        check_columns(atl08df, {
            "time_ns",
            "latitude",
            "longitude",
            "segment_id_beg",
            "segment_landcover",
            "segment_snowcover",
            "n_seg_ph",
            "solar_elevation",
            "terrain_slope",
            "n_te_photons",
            "h_te_uncertainty",
            "h_te_median",
            "h_canopy",
            "h_canopy_uncertainty",
            "segment_cover",
            "n_ca_photons",
            "h_max_canopy",
            "h_min_canopy",
            "h_mean_canopy",
            "canopy_openness",
            "canopy_h_metrics"
        })

        -- fixed sample (first row) for known granule ATL08_20200307004141_10890603_007_01.h5
        local idx = 1
        check_expected({
            time_ns = 1583541719170012416,
            latitude = 60.644901275634766,
            longitude = -145.39999389648438,
            segment_id_beg = 337095,
            segment_landcover = 126,
            segment_snowcover = 2,
            n_seg_ph = 345,
            solar_elevation = 16.451,
            terrain_slope = -0.004300970584154129,
            n_te_photons = 68,
            h_te_uncertainty = 3.91519,
            h_te_median = 266.374,
            h_canopy = 14.0389404296875,
            h_canopy_uncertainty = 4.948657512664795,
            segment_cover = 61,
            n_ca_photons = 124,
            h_max_canopy = 14.923,
            h_min_canopy = 0.527679,
            h_mean_canopy = 4.07945,
            canopy_openness = 3.68869
        }, atl08df, idx, 0.001)

        local metrics = atl08df["canopy_h_metrics"][idx]
        runner.assert(#metrics == 18, string.format("canopy_h_metrics length mismatch: %d", #metrics))
        runner.assert(math.abs(metrics[1] - 0.89624) <= 0.001, string.format("canopy_h_metrics[1] => %f", metrics[1]))
        runner.assert(math.abs(metrics[18] - 12.1895) <= 0.001, string.format("canopy_h_metrics[18] => %f", metrics[18]))

    end

    runner.assert(atl08df:meta("granule") == parms["resource"], "granule name mismatch")
end)

-- Self Test --

runner.unittest("ATL08 DataFrame - Quality Filters", function()

    local base_parms = icesat2.parms({
        beams = "gt3r",
        resource = "ATL08_20200307004141_10890603_007_01.h5"
    })

    local atl08h5 = h5.object(asset_name, base_parms["resource"])
    local base_df = icesat2.atl08x("gt3r", base_parms, atl08h5, core.EVENTQ)

    runner.assert(base_df:waiton(60000), "timed out creating dataframe", true)
    runner.assert(base_df:inerror() == false, "dataframe encountered error")
    runner.assert(base_df:numrows() == 10214, string.format("incorrect number of rows: %d", base_df:numrows()))
    runner.assert(base_df:numcols() == 21, string.format("incorrect number of columns: %d", base_df:numcols()))

    local te_parms = icesat2.parms({
        beams = "gt3r",
        resource = "ATL08_20200307004141_10890603_007_01.h5",
        phoreal = { te_quality_filter = 70 }
    })
    local te_df = icesat2.atl08x("gt3r", te_parms, atl08h5, core.EVENTQ)
    runner.assert(te_df:waiton(60000), "timed out creating dataframe", true)
    runner.assert(te_df:inerror() == false, "dataframe encountered error")
    runner.assert(te_df:numrows() == 9837, string.format("incorrect number of rows: %d", te_df:numrows()))
    runner.assert(te_df:numcols() == 22, string.format("incorrect number of columns: %d", te_df:numcols()))
    check_columns(te_df, {"te_quality_score"})

    local can_parms = icesat2.parms({
        beams = "gt3r",
        resource = "ATL08_20200307004141_10890603_007_01.h5",
        phoreal = { can_quality_filter = 75 }
    })
    local can_df = icesat2.atl08x("gt3r", can_parms, atl08h5, core.EVENTQ)
    runner.assert(can_df:waiton(60000), "timed out creating dataframe", true)
    runner.assert(can_df:inerror() == false, "dataframe encountered error")
    runner.assert(can_df:numrows() == 6486, string.format("incorrect number of rows: %d", can_df:numrows()))
    runner.assert(can_df:numcols() == 22, string.format("incorrect number of columns: %d", can_df:numcols()))
    check_columns(can_df, {"can_quality_score"})

    local both_parms = icesat2.parms({
        beams = "gt3r",
        resource = "ATL08_20200307004141_10890603_007_01.h5",
        phoreal = { te_quality_filter = 70, can_quality_filter = 75 }
    })
    local both_df = icesat2.atl08x("gt3r", both_parms, atl08h5, core.EVENTQ)
    runner.assert(both_df:waiton(60000), "timed out creating dataframe", true)
    runner.assert(both_df:inerror() == false, "dataframe encountered error")
    runner.assert(both_df:numrows() == 6412, string.format("incorrect number of rows: %d", both_df:numrows()))
    runner.assert(both_df:numcols() == 23, string.format("incorrect number of columns: %d", both_df:numcols()))
    check_columns(both_df, {"te_quality_score", "can_quality_score"})

    local zero_parms = icesat2.parms({
        beams = "gt3r",
        resource = "ATL08_20200307004141_10890603_007_01.h5",
        phoreal = { te_quality_filter = 0, can_quality_filter = 0 }
    })
    local zero_df = icesat2.atl08x("gt3r", zero_parms, atl08h5, core.EVENTQ)
    runner.assert(zero_df:waiton(60000), "timed out creating dataframe", true)
    runner.assert(zero_df:inerror() == false, "dataframe encountered error")
    runner.assert(zero_df:numrows() == 10214, string.format("incorrect number of rows: %d", zero_df:numrows()))
    runner.assert(zero_df:numcols() == 23, string.format("incorrect number of columns: %d", zero_df:numcols()))
    check_columns(zero_df, {"te_quality_score", "can_quality_score"})

end)

-- Self Test --

runner.unittest("ATL08 DataFrame - Ancillary Data", function()

    local parms = icesat2.parms({
        beams = "gt3r",
        resource = "ATL08_20200307004141_10890603_007_01.h5",
        atl08_fields = {"beam_azimuth", "segment_watermask"}
    })

    local atl08h5 = h5.object(asset_name, parms["resource"])
    local atl08df = icesat2.atl08x("gt3r", parms, atl08h5, core.EVENTQ)

    runner.assert(atl08df:waiton(60000), "timed out creating dataframe", true)
    runner.assert(atl08df:inerror() == false, "dataframe encountered error")

    runner.assert(atl08df:numrows() == 10214, "incorrect rows returned for ATL08 ancillary dataframe")

    check_columns(atl08df, {
        "beam_azimuth",
        "segment_watermask"
    })

    local idx = 1
    check_expected({
        beam_azimuth = -1.29266,
        segment_watermask = 0
    }, atl08df, idx, 0.001)

end)

-- Self Test --

runner.unittest("ATL08 Earthdata Granules - Time Filtered", function()

    local poly = {
        { lon = -46.15177435697, lat = 75.59406753389 },
        { lon = -46.15177435697, lat = 75.631128372239 },
        { lon = -46.21794107665, lat = 75.631128372239 },
        { lon = -46.21794107665, lat = 75.59406753389 },
        { lon = -46.15177435697, lat = 75.59406753389 }
    }

    local max_resources = 200
    local rc, granules = earthdata.search({
        asset = "icesat2-atl08",
        poly = poly,
        t0 = "2021-01-01",
        t1 = "2021-05-31",
        max_resources = max_resources
    })

    runner.assert(rc == earthdata.SUCCESS, string.format("earthdata search failed: %d", rc))
    runner.assert(type(granules) == "table", "earthdata search returned non-table granules")
    runner.assert(#granules > 0, "earthdata search returned zero granules")
    runner.assert(#granules <= max_resources, "earthdata search exceeded max_resources")

    table.sort(granules)
    local expected = {
        "ATL08_20210107161057_02241005_007_01.h5",
        "ATL08_20210205144704_06661005_007_01.h5",
        "ATL08_20210314235557_12371003_007_01.h5",
        "ATL08_20210408115050_02241105_007_01.h5",
        "ATL08_20210412223157_02921103_007_01.h5",
        "ATL08_20210507102653_06661105_007_01.h5"
    }
    runner.assert(#granules == #expected, string.format("unexpected granule count: %d", #granules))
    for i = 1, #expected do
        runner.assert(granules[i] == expected[i], string.format("granule[%d] mismatch: %s", i, granules[i] or "nil"))
    end

end)

-- Report Results --

runner.report()
