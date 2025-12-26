local runner = require("test_executive")

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
            "extent_id",
            "delta_time",
            "latitude",
            "longitude",
            "segment_id_beg",
            "segment_id_end",
            "night_flag",
            "n_seg_ph",
            "solar_elevation",
            "solar_azimuth",
            "terrain_flg",
            "brightness_flag",
            "cloud_flag_atm",
            "h_te_best_fit",
            "h_te_interp",
            "terrain_slope",
            "n_te_photons",
            "te_quality_score",
            "h_te_uncertainty",
            "h_canopy",
            "h_canopy_abs",
            "h_canopy_uncertainty",
            "segment_cover",
            "n_ca_photons",
            "can_quality_score"
        })

        -- fixed sample (first row) for known granule ATL08_20200307004141_10890603_007_01.h5
        local idx = 1
        check_expected({
            time_ns = 1583541719170012416,
            latitude = 60.644901275634766,
            longitude = -145.39999389648438,
            delta_time = 6.877691917001235e7,
            segment_id_beg = 337095,
            segment_id_end = 337099,
            night_flag = 0,
            n_seg_ph = 345,
            solar_elevation = 16.451,
            solar_azimuth = 224.372,
            terrain_flg = 1,
            brightness_flag = 0,
            cloud_flag_atm = 2,
            h_te_best_fit = 266.6724548339844,
            h_te_interp = 266.65447998046875,
            terrain_slope = -0.004300970584154129,
            n_te_photons = 68,
            te_quality_score = 70,
            h_te_uncertainty = 3.91519,
            h_canopy = 14.0389404296875,
            h_canopy_abs = 280.7113952636719,
            h_canopy_uncertainty = 4.948657512664795,
            segment_cover = 61,
            n_ca_photons = 124,
            can_quality_score = 75
        }, atl08df, idx, 0.001)

    end

    runner.assert(atl08df:meta("granule") == parms["resource"], "granule name mismatch")
end)

-- Self Test --

runner.unittest("ATL08 DataFrame - Ancillary Data", function()

    local parms = icesat2.parms({
        beams = "gt3r",
        resource = "ATL08_20200307004141_10890603_007_01.h5",
        atl08_fields = {"segment_landcover", "segment_snowcover"}
    })

    local atl08h5 = h5.object(asset_name, parms["resource"])
    local atl08df = icesat2.atl08x("gt3r", parms, atl08h5, core.EVENTQ)

    runner.assert(atl08df:waiton(60000), "timed out creating dataframe", true)
    runner.assert(atl08df:inerror() == false, "dataframe encountered error")

    runner.assert(atl08df:numrows() == 10214, "incorrect rows returned for ATL08 ancillary dataframe")

    check_columns(atl08df, {
        "segment_landcover",
        "segment_snowcover"
    })

    local idx = 1
    check_expected({
        segment_landcover = 126,
        segment_snowcover = 2
    }, atl08df, idx, 0)

end)

-- Report Results --

runner.report()
