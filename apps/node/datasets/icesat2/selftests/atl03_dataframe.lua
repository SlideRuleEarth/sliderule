local runner = require("test_executive")
local prettyprint = require("prettyprint")

-- Requirements --

if (not sys.getcfg("in_cloud") and not runner.isglobal()) then
    return runner.skip()
end

-- Setup --

runner.authenticate({'nsidc-cloud'})
local asset_name = "icesat2"

-- Helper Function --

local function check_expected(exp, df, index, t)
    for key,value in pairs(exp) do
        if index then
            runner.assert(math.abs(df[key][index] - value) <= t, string.format("%s[%d] => %f", key, index, df[key][index]))
        else
            runner.assert(math.abs(df:meta(key) - value) <= t, string.format("%s => %f", key, df:meta(key)))
        end
    end
end

-- Self Test --

runner.unittest("ATL03 DataFrame", function()

    local parms = icesat2.parms03({
        srt = 3,
        cnf = 4,
        resource = "ATL03_20200304065203_10470605_006_01.h5"
    }, nil, "icesat2")

    local atl03h5 = h5coro.object(asset_name, parms["resource"])
    local atl03df = icesat2.atl03x("gt1l", parms, atl03h5, nil, nil, core.EVENTQ)

    runner.assert(atl03df:waiton(30000), "timed out creating dataframe", true)
    runner.assert(atl03df:inerror() == false, "dataframe encountered error")

    runner.assert(atl03df:numrows() == 5912939, string.format("incorrect number of rows: %d", atl03df:numrows()))
    runner.assert(atl03df:numcols() == 13, string.format("incorrect number of columns: %d", atl03df:numcols()))

    check_expected({
        time_ns = 1583304724130344448,
        latitude = 79.993572,
        longitude = -40.942408,
        x_atc = 11132842.088085,
        y_atc = 3271.814941,
        height = 2178.863281,
        solar_elevation = -11.243111,
        background_rate = 32401.623047,
        spacecraft_velocity = 7096.781738,
        atl03_cnf = 4,
        quality_ph = 0,
        ph_index = 112
    }, atl03df, 100, 0.00001)

    check_expected({
        spot = 6,
        cycle = 6,
        region = 5,
        rgt = 1047,
        gt = 10
    }, atl03df, nil, 0)

end)

-- Self Test --

runner.unittest("ATL03 DataFrame - Ancillary Data", function()

    local parms = icesat2.parms03({
        srt = 3,
        cnf = 4,
        resource = "ATL03_20200304065203_10470605_006_01.h5",
        atl03_geo_fields = {"knn", "pitch"},
        atl03_corr_fields = {"geoid"},
        atl03_ph_fields = {"ph_id_channel", "ph_id_pulse"},
        atl08_fields = {"h_dif_ref", "rgt", "sigma_atlas_land", "cloud_flag_atm"}
    }, nil, "icesat2")

    local atl03h5 = h5coro.object(asset_name, parms["resource"])
    local atl08h5 = h5coro.object(asset_name, "ATL08_20200304065203_10470605_006_01.h5")
    local atl03df = icesat2.atl03x("gt2r", parms, atl03h5, atl08h5, nil, core.EVENTQ)

    runner.assert(atl03df:waiton(240000), "timed out creating dataframe", true)
    runner.assert(atl03df:inerror() == false, "dataframe encountered error")

    runner.assert(atl03df:numrows() == 19522774, string.format("incorrect number of rows: %d", atl03df:numrows()))
    runner.assert(atl03df:numcols() == 23, string.format("incorrect number of columns: %d", atl03df:numcols()))

    check_expected({
        time_ns = 1583304724455644416,
        latitude = 80.000077,
        longitude = -41.109609,
        x_atc = 11132821.369912,
        y_atc = -52.097466,
        height = 2180.452148,
        solar_elevation = -11.265012,
        background_rate = 10853.832031,
        spacecraft_velocity = 7096.785645,
        atl08_class = 1,
        atl03_cnf = 4,
        quality_ph = 0,
        ph_index = 108,
        knn = 14,
        pitch = -0.049935,
        geoid = 33.014797,
        ph_id_channel = 92,
        ph_id_pulse = 83,
        h_dif_ref = 0.676025,
        rgt = 1047,
        sigma_atlas_land = 0.130923,
        cloud_flag_atm = 0
    }, atl03df, 100, 0.00001)

    check_expected({
        spot = 3,
        cycle = 6,
        region = 5,
        rgt = 1047,
        gt = 40
    }, atl03df, nil, 0)

end, {"long"})

-- Self Test --

runner.unittest("ATL06 Surface Fitter", function()

    local parms = icesat2.parms03({
        srt = 3,
        cnf = 4,
        resource = "ATL03_20200304065203_10470605_006_01.h5",
        fit = { maxi = 2 }
    }, nil, "icesat2")

    local atl03h5 = h5coro.object(asset_name, parms["resource"])
    local df = icesat2.atl03x("gt1l", parms, atl03h5, nil, nil, core.EVENTQ)
    local fitter = icesat2.fit(parms)

    df:run(fitter)
    df:run(core.TERMINATE)

    runner.assert(df:finished(30000), "failed to wait for dataframe to finish")
    runner.assert(df:inerror() == false, "dataframe encountered error")

    runner.assert(df:numrows() == 98924, string.format("incorrect number of rows: %d", df:numrows()))
    runner.assert(df:numcols() == 14, string.format("incorrect number of columns: %d", df:numcols()))

    prettyprint.display(df:row(100))

    check_expected({
        time_ns = 1583304724408144640,
        latitude = 79.976320,
        longitude = -40.964091,
        x_atc = 11134813.724491,
        y_atc = 3272.001221,
        h_mean = 2183.448242,
        dh_fit_dx = -0.000171,
        w_surface_window_final = 3.0,
        rms_misfit = 0.138086,
        h_sigma = 0.012963,
        photon_start = 6861,
        n_fit_photons = 115,
        pflags = 0,
    }, df, 100, 0.00001)

    check_expected({
        spot = 6,
        cycle = 6,
        region = 5,
        rgt = 1047,
        gt = 10
    }, df, nil, 0)

end, {"long"})

-- Self Test --

runner.unittest("ATL03 DataFrame - YAPC Score Filter", function()

    local poly = {
        { lon = -42.0, lat = 79.9 },
        { lon = -40.0, lat = 79.9 },
        { lon = -40.0, lat = 80.1 },
        { lon = -42.0, lat = 80.1 },
        { lon = -42.0, lat = 79.9 }
    }

    local function count_rows(score)
        local parms = icesat2.parms03({
            srt = 3,
            cnf = -2,
            poly = poly,
            yapc = { version = 0, score = score },
            resource = "ATL03_20200304065203_10470605_007_01.h5"
        }, nil, "icesat2")
        local atl03h5 = h5coro.object(asset_name, parms["resource"])
        local df = icesat2.atl03x("gt1l", parms, atl03h5, nil, nil, core.EVENTQ)
        runner.assert(df:waiton(240000), "timed out creating dataframe", true)
        runner.assert(df:inerror() == false, "dataframe encountered error")
        local min_score = 65535
        for i = 1, math.min(df:numrows(), 1000) do
            if df["yapc_score"][i] < min_score then
                min_score = df["yapc_score"][i]
            end
        end
        return df:numrows(), min_score
    end

    local rows_all, min_score_all = count_rows(0)
    local rows_mid, min_score_mid = count_rows(1000)
    local rows_high, min_score_high = count_rows(8000)

    runner.assert(rows_all > 0, "no photons returned for score 0")
    runner.assert(rows_mid <= rows_all, string.format("score 1000 did not filter photons: %d > %d", rows_mid, rows_all))
    runner.assert(rows_high < rows_all, string.format("score 8000 did not filter photons: %d >= %d", rows_high, rows_all))
    runner.assert(rows_high <= rows_mid, string.format("score threshold not monotonic: %d > %d", rows_high, rows_mid))
    runner.assert(min_score_mid >= 1000, string.format("photon below score threshold returned: %d < 1000", min_score_mid))
    runner.assert(min_score_high >= 8000, string.format("photon below score threshold returned: %d < 8000", min_score_high))
    print(string.format("rows: %d (score 0, min %d), %d (score 1000, min %d), %d (score 8000, min %d)",
        rows_all, min_score_all, rows_mid, min_score_mid, rows_high, min_score_high))

end)

-- Self Test --

runner.unittest("ATL03 DataFrame - YAPC Unsupported Version", function()

    local parms = icesat2.parms03({
        srt = 3,
        cnf = 4,
        yapc = { version = 3, score = 0 },
        resource = "ATL03_20200304065203_10470605_007_01.h5"
    }, nil, "icesat2")

    local atl03h5 = h5coro.object(asset_name, parms["resource"])
    local df = icesat2.atl03x("gt1l", parms, atl03h5, nil, nil, core.EVENTQ)

    runner.assert(df:waiton(30000), "timed out creating dataframe", true)
    runner.assert(df:numrows() == 0, string.format("photons returned for unsupported yapc version: %d", df:numrows()))

end)

-- Self Test --

runner.unittest("ATL03 DataFrame - Signal Classification Filter", function()

    local poly = {
        { lon = -42.0, lat = 79.9 },
        { lon = -40.0, lat = 79.9 },
        { lon = -40.0, lat = 80.1 },
        { lon = -42.0, lat = 80.1 },
        { lon = -42.0, lat = 79.9 }
    }

    -- unfiltered baseline
    local parms_all = icesat2.parms03({
        srt = 3,
        cnf = -2,
        poly = poly,
        resource = "ATL03_20200304065203_10470605_007_01.h5"
    }, nil, "icesat2")
    local atl03h5 = h5coro.object(asset_name, parms_all["resource"])
    local df_all = icesat2.atl03x("gt1l", parms_all, atl03h5, nil, nil, core.EVENTQ)
    runner.assert(df_all:waiton(240000), "timed out creating dataframe", true)
    runner.assert(df_all:inerror() == false, "dataframe encountered error")

    -- filtered on highest reflecting surface
    local parms_sel = icesat2.parms03({
        srt = 3,
        cnf = -2,
        poly = poly,
        atl03_signal_class = { "primary_signal", "fitted_signal" },
        resource = "ATL03_20200304065203_10470605_007_01.h5"
    }, nil, "icesat2")
    local atl03h5_sel = h5coro.object(asset_name, parms_sel["resource"])
    local df_sel = icesat2.atl03x("gt1l", parms_sel, atl03h5_sel, nil, nil, core.EVENTQ)
    runner.assert(df_sel:waiton(240000), "timed out creating dataframe", true)
    runner.assert(df_sel:inerror() == false, "dataframe encountered error")

    runner.assert(df_sel:numrows() > 0, "no photons returned for signal class selection")
    runner.assert(df_sel:numrows() < df_all:numrows(), string.format("signal class selection did not filter photons: %d >= %d", df_sel:numrows(), df_all:numrows()))
    runner.assert(df_sel:numcols() == df_all:numcols() + 1, string.format("incorrect number of columns: %d", df_sel:numcols()))
    for i = 1, math.min(df_sel:numrows(), 1000) do
        local signal_class = df_sel["atl03_signal_class"][i]
        runner.assert(signal_class == 4 or signal_class == 5, string.format("unselected signal class returned: %d", signal_class))
    end

    -- explicitly selecting every classification is equivalent to the default:
    -- signal_class_ph is not read and no column is added
    local parms_every = icesat2.parms03({
        srt = 3,
        cnf = -2,
        poly = poly,
        atl03_signal_class = { "ignored", "likely_noise", "likely_signal", "signal_below", "signal_above", "primary_signal", "fitted_signal" },
        resource = "ATL03_20200304065203_10470605_007_01.h5"
    }, nil, "icesat2")
    local atl03h5_every = h5coro.object(asset_name, parms_every["resource"])
    local df_every = icesat2.atl03x("gt1l", parms_every, atl03h5_every, nil, nil, core.EVENTQ)
    runner.assert(df_every:waiton(240000), "timed out creating dataframe", true)
    runner.assert(df_every:numrows() == df_all:numrows(), string.format("selecting every signal class changed the photon count: %d ~= %d", df_every:numrows(), df_all:numrows()))
    runner.assert(df_every:numcols() == df_all:numcols(), string.format("selecting every signal class added a column: %d ~= %d", df_every:numcols(), df_all:numcols()))

    -- selection is rejected for pre-007 granules
    local parms_006 = icesat2.parms03({
        srt = 3,
        cnf = -2,
        poly = poly,
        atl03_signal_class = { "primary_signal", "fitted_signal" },
        resource = "ATL03_20200304065203_10470605_006_01.h5"
    }, nil, "icesat2")
    local atl03h5_006 = h5coro.object(asset_name, parms_006["resource"])
    local df_006 = icesat2.atl03x("gt1l", parms_006, atl03h5_006, nil, nil, core.EVENTQ)
    runner.assert(df_006:waiton(30000), "timed out creating dataframe", true)
    runner.assert(df_006:numrows() == 0, string.format("photons returned for pre-007 signal class selection: %d", df_006:numrows()))

end)

-- Self Test --

runner.unittest("ATL06 Surface Fitter - Signal Classification", function()

    local poly = {
        { lon = -42.0, lat = 79.9 },
        { lon = -40.0, lat = 79.9 },
        { lon = -40.0, lat = 80.1 },
        { lon = -42.0, lat = 80.1 },
        { lon = -42.0, lat = 79.9 }
    }

    local function fit_rows(signal_class)
        local parms = icesat2.parms03({
            srt = 3,
            cnf = -2,
            poly = poly,
            atl03_signal_class = signal_class,
            fit = { maxi = 2 },
            resource = "ATL03_20200304065203_10470605_007_01.h5"
        }, nil, "icesat2")
        local atl03h5 = h5coro.object(asset_name, parms["resource"])
        local df = icesat2.atl03x("gt1l", parms, atl03h5, nil, nil, core.EVENTQ)
        local fitter = icesat2.fit(parms)
        df:run(fitter)
        df:run(core.TERMINATE)
        runner.assert(df:finished(240000), "failed to wait for dataframe to finish")
        runner.assert(df:inerror() == false, "dataframe encountered error")
        return df:numrows()
    end

    local rows_all = fit_rows(nil)
    local rows_sel = fit_rows({"primary_signal", "fitted_signal"})

    runner.assert(rows_all > 0, "no elevations returned for unfiltered fit")
    runner.assert(rows_sel > 0, "no elevations returned for signal class fit")
    -- fit row counts are not monotonic in photon count: noise photons make
    -- segments fail the robust fit, so a cleaner selection can yield more
    -- elevations, not fewer (on this granule/polygon cnf=4 alone gives 527
    -- rows vs 415 for cnf=0). Only check that the selection changed the fit.
    runner.assert(rows_sel ~= rows_all, string.format("signal class selection had no effect on fit: %d == %d", rows_sel, rows_all))

end, {"long"})

-- Report Results --

runner.report()

