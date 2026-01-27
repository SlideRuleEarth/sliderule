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
runner.unittest("ATL06 DataFrame", function()

    local parms = icesat2.parms({
        srt = 3,
        cnf = 4,
        resource = "ATL06_20200303180710_10390603_007_01.h5"
    })

    local atl06h5 = h5.object(asset_name, parms["resource"])
    local atl06df = icesat2.atl06x("gt1l", parms, atl06h5, core.EVENTQ)

    runner.assert(atl06df:waiton(60000), "timed out creating dataframe", true)
    runner.assert(atl06df:inerror() == false, "dataframe encountered error")

    -- Expected shape for ATL06_20200303180710_10390603_007_01.h5 / gt1l
    runner.assert(atl06df:numrows() == 114336, string.format("incorrect number of rows: %d", atl06df:numrows()))
    runner.assert(atl06df:numcols() == 19, string.format("incorrect number of columns: %d", atl06df:numcols()))

    if atl06df:numrows() >= 1 then
        check_columns(atl06df, {
            "time_ns",
            "latitude",
            "longitude",
            "x_atc",
            "y_atc",
            "h_li",
            "h_li_sigma",
            "sigma_geo_h",
            "atl06_quality_summary",
            "segment_id",
            "seg_azimuth",
            "dh_fit_dx",
            "h_robust_sprd",
            "w_surface_window_final",
            "bsnow_conf",
            "bsnow_h",
            "r_eff",
            "tide_ocean",
            "n_fit_photons"
        })

        -- fixed sample (first fully finite row) for known granule ATL06_20200303180710_10390603_007_01.h5
        local idx = 16
        check_expected({
            time_ns = 1583258831569810944,
            latitude = 59.529772793835,
            longitude = -44.325544216762,
            x_atc = 6633831.0618938,
            y_atc = 3260.5561523438,
            h_li = 40.553344726562,
            h_li_sigma = 0.41705507040024,
            sigma_geo_h = 0.28224530816078,
            atl06_quality_summary = 0,
            segment_id = 330916,
            seg_azimuth = -5.8324255943298,
            dh_fit_dx = -0.047380782663822,
            h_robust_sprd = 0.50711327791214,
            w_surface_window_final = 3.0426795482635,
            bsnow_conf = 127,
            bsnow_h = 29.979246139526,
            r_eff = 0.10466815531254,
            tide_ocean = -0.25118598341942,
            n_fit_photons = 14
        }, atl06df, idx, 0.00001)

        check_expected({
            spot = 6,
            cycle = 6,
            region = 3,
            rgt = 1039,
            gt = 10
        }, atl06df, nil, 0)
    end

    runner.assert(atl06df:meta("granule") == parms["resource"], "granule name mismatch")
end)

-- Self Test --

runner.unittest("ATL06 DataFrame - Ancillary Data", function()

    local parms = icesat2.parms({
        srt = 3,
        cnf = 4,
        resource = "ATL06_20200303180710_10390603_007_01.h5",
        atl06_fields = {"dem/dem_flag", "dem/dem_h"}
    })

    local atl06h5 = h5.object(asset_name, parms["resource"])
    local atl06df = icesat2.atl06x("gt2r", parms, atl06h5, core.EVENTQ)

    runner.assert(atl06df:waiton(60000), "timed out creating dataframe", true)
    runner.assert(atl06df:inerror() == false, "dataframe encountered error")

    runner.assert(atl06df:numrows() == 114605, "incorrect rows returned for ATL06 ancillary dataframe")
    runner.assert(atl06df:numcols() == 21, string.format("incorrect columns: %d", atl06df:numcols()))

    local idx = 1
    check_expected({
        ["dem/dem_flag"] = 3,
        ["dem/dem_h"] = 42.131591796875
    }, atl06df, idx, 0.00001)

end)

-- Self Test --

runner.unittest("ATL06 Earthdata Granules - Time Filtered", function()

    local poly = {
        { lon = -46.15177435697, lat = 75.59406753389 },
        { lon = -46.15177435697, lat = 75.631128372239 },
        { lon = -46.21794107665, lat = 75.631128372239 },
        { lon = -46.21794107665, lat = 75.59406753389 },
        { lon = -46.15177435697, lat = 75.59406753389 }
    }

    local max_resources = 200
    local rc, granules = earthdata.search({
        asset = "icesat2-atl06",
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
        "ATL06_20210107161057_02241005_007_01.h5",
        "ATL06_20210205144704_06661005_007_01.h5",
        "ATL06_20210314235557_12371003_007_01.h5",
        "ATL06_20210408115050_02241105_007_01.h5",
        "ATL06_20210412223157_02921103_007_01.h5",
        "ATL06_20210507102653_06661105_007_01.h5"
    }
    runner.assert(#granules == #expected, string.format("unexpected granule count: %d", #granules))
    for i = 1, #expected do
        runner.assert(granules[i] == expected[i], string.format("granule[%d] mismatch: %s", i, granules[i] or "nil"))
    end

end)

-- Report Results --

runner.report()
