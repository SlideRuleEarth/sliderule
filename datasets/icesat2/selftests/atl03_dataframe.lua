local runner = require("test_executive")
local prettyprint = require("prettyprint")

-- Requirements --

if (not sys.getcfg("in_cloud") and not runner.isglobal()) then
    return runner.skip()
end

-- Setup --

runner.authenticate()
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

    local parms = icesat2.parms({
        srt = 3,
        cnf = 4,
        resource = "ATL03_20200304065203_10470605_006_01.h5"
    })

    local atl03h5 = h5.object(asset_name, parms["resource"])
    local atl03df = icesat2.atl03x("gt1l", parms, atl03h5, nil, nil, core.EVENTQ)

    runner.assert(atl03df:waiton(30000), "timed out creating dataframe", true)
    runner.assert(atl03df:inerror() == false, "dataframe encountered error")

    runner.assert(atl03df:numrows() == 5912939, string.format("incorrect number of rows: %d", atl03df:numrows()))
    runner.assert(atl03df:numcols() == 12, string.format("incorrect number of columns: %d", atl03df:numcols()))

    check_expected({
        time_ns = 1583304724130344448,
        latitude = 79.993572,
        longitude = -40.942408,
        x_atc = 11132842.088085,
        y_atc = 3271.814941,
        height = 2178.863281,
        solar_elevation = -11.243111,
        background_rate = 33019.824219,
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

    local parms = icesat2.parms({
        srt = 3,
        cnf = 4,
        resource = "ATL03_20200304065203_10470605_006_01.h5",
        atl03_geo_fields = {"knn", "pitch"},
        atl03_corr_fields = {"geoid"},
        atl03_ph_fields = {"ph_id_channel", "ph_id_pulse"},
        atl08_fields = {"h_dif_ref", "rgt", "sigma_atlas_land", "cloud_flag_atm"}
    })

    local atl03h5 = h5.object(asset_name, parms["resource"])
    local atl08h5 = h5.object(asset_name, "ATL08_20200304065203_10470605_006_01.h5")
    local atl03df = icesat2.atl03x("gt2r", parms, atl03h5, atl08h5, nil, core.EVENTQ)

    runner.assert(atl03df:waiton(240000), "timed out creating dataframe", true)
    runner.assert(atl03df:inerror() == false, "dataframe encountered error")

    runner.assert(atl03df:numrows() == 19522774, string.format("incorrect number of rows: %d", atl03df:numrows()))
    runner.assert(atl03df:numcols() == 22, string.format("incorrect number of columns: %d", atl03df:numcols()))

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

end)

-- Self Test --

runner.unittest("ATL06 Surface Fitter", function()

    local parms = icesat2.parms({
        srt = 3,
        cnf = 4,
        resource = "ATL03_20200304065203_10470605_006_01.h5",
        fit = { maxi = 2 }
    })

    local atl03h5 = h5.object(asset_name, parms["resource"])
    local df = icesat2.atl03x("gt1l", parms, atl03h5, nil, nil, core.EVENTQ)
    local fitter = icesat2.fit(parms)

    df:run(fitter)
    df:run(core.TERMINATE)

    runner.assert(df:finished(30000, rspq), "failed to wait for dataframe to finish")
    runner.assert(df:inerror() == false, "dataframe encountered error")

    runner.assert(df:numrows() == 99364, string.format("incorrect number of rows: %d", df:numrows()))
    runner.assert(df:numcols() == 13, string.format("incorrect number of columns: %d", df:numcols()))

    prettyprint.display(df:row(100))

    check_expected({
        time_ns = 1583304724417144064,
        latitude = 79.975761143791,
        longitude = -40.964800696036,
        x_atc = 11134877.680426,
        y_atc = 3271.8671875,
        h_mean = 2183.6225845824,
        dh_fit_dx = 0.0041639762930572,
        window_height = 3.0,
        rms_misfit = 0.12183286994696,
        h_sigma = 0.010312998667359,
        photon_start = 7076,
        photon_count = 140,
        pflags = 0,
    }, df, 100, 0.00001)

    check_expected({
        spot = 6,
        cycle = 6,
        region = 5,
        rgt = 1047,
        gt = 10
    }, df, nil, 0)

end)

-- Report Results --

runner.report()

