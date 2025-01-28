local runner = require("test_executive")
local asset = require("asset")
local json = require("json")
local prettyprint = require("prettyprint")

-- Setup Logging --

local console = require("console")
console.monitor:config(core.LOG, core.INFO)
sys.setlvl(core.LOG, core.INFO)

-- Setup Credentials --

local assets = asset.loaddir()
local asset_name = "icesat2"
local nsidc_s3 = core.getbyname(asset_name)
local name, identity, driver = nsidc_s3:info()
local creds = aws.csget(identity)
if not creds then
    local earthdata_url = "https://data.nsidc.earthdatacloud.nasa.gov/s3credentials"
    local response, _ = core.get(earthdata_url)
    local _, credential = pcall(json.decode, response)
    aws.csput(identity, credential)
end

-- Helper Function --

local function check_expected(exp, df, index, t)
    for key,value in pairs(exp) do
        if index then
            runner.check(math.abs(df[key][index] - value) <= t, string.format("%s[%d] => %f", key, index, df[key][index]))
        else
            runner.check(math.abs(df:meta(key) - value) <= t, string.format("%s => %f", key, df:meta(key)))
        end
    end
end

-- Unit Test --

runner.unittest("ATL03 DataFrame", function()

    local parms = icesat2.parms({
        cnf = 4,
        resource = "ATL03_20200304065203_10470605_006_01.h5"
    })

    local atl03h5 = h5.object(asset_name, parms["resource"])
    local atl03df = icesat2.atl03x("gt1l", parms, atl03h5, nil, core.EVENTQ)

    runner.check(atl03df:waiton(10000), "failed to create dataframe", true)
    runner.check(atl03df:inerror() == false, "dataframe encountered error")

    runner.check(atl03df:numrows() == 5912939, string.format("incorrect number of rows: %d", atl03df:numrows()))
    runner.check(atl03df:numcols() == 17, string.format("incorrect number of columns: %d", atl03df:numcols()))

    check_expected({
        time_ns = 1583304724130344448,
        latitude = 79.993572,
        longitude = -40.942408,
        x_atc = 11132842.088085,
        y_atc = 3271.814941,
        height = 2178.863281,
        relief = 0.0,
        solar_elevation = -11.243111,
        background_rate = 33019.825791,
        spacecraft_velocity = 7096.781738,
        landcover = 255,
        snowcover = 255,
        atl08_class = 4,
        atl03_cnf = 4,
        quality_ph = 0,
        yapc_score = 0,
        segment_id = 555765
    }, atl03df, 100, 0.00001)

    check_expected({
        spot = 6,
        cycle = 6,
        region = 5,
        reference_ground_track = 1047,
        spacecraft_orientation = 1
    }, atl03df, nil, 0)

end)

-- Unit Test --

runner.unittest("ATL03 DataFrame - Ancillary Data", function()

    local parms = icesat2.parms({
        cnf = 4,
        resource = "ATL03_20200304065203_10470605_006_01.h5",
        atl03_geo_fields = {"knn", "pitch"},
        atl03_corr_fields = {"geoid"},
        atl03_ph_fields = {"ph_id_channel", "ph_id_pulse"},
        atl08_fields = {"h_dif_ref", "rgt", "sigma_atlas_land", "cloud_flag_atm"}
    })

    local atl03h5 = h5.object(asset_name, parms["resource"])
    local atl08h5 = h5.object(asset_name, "ATL08_20200304065203_10470605_006_01.h5")
    local atl03df = icesat2.atl03x("gt2r", parms, atl03h5, atl08h5, core.EVENTQ)

    runner.check(atl03df:waiton(30000), "failed to create dataframe", true)
    runner.check(atl03df:inerror() == false, "dataframe encountered error")

    runner.check(atl03df:numrows() == 19522774, string.format("incorrect number of rows: %d", atl03df:numrows()))
    runner.check(atl03df:numcols() == 26, string.format("incorrect number of columns: %d", atl03df:numcols()))

    check_expected({
        time_ns = 1583304724455644416,
        latitude = 80.000077,
        longitude = -41.109609,
        x_atc = 11132821.369912,
        y_atc = -52.097466,
        height = 2180.452148,
        relief = 0.0,
        solar_elevation = -11.265012,
        background_rate = 10853.832031,
        spacecraft_velocity = 7096.785645,
        landcover = 255,
        snowcover = 255,
        atl08_class = 1,
        atl03_cnf = 4,
        quality_ph = 0,
        yapc_score = 0,
        segment_id = 555764,
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
        reference_ground_track = 1047,
        spacecraft_orientation = 1
    }, atl03df, nil, 0)

end)

-- Report Results --

runner.report()

