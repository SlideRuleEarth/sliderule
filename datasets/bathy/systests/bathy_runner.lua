local json      = require("json")
local earthdata = require("earth_data_query")
local console   = require("console")
local asset     = require("asset")
local runner    = require("container_runtime")

-- configure logging
console.monitor:config(core.LOG, core.INFO)
sys.setlvl(core.LOG, core.INFO)

-- load assets
local _assets = asset.loaddir()

local _role_auth_script = core.script("iam_role_auth"):name("RoleAuthScript")
while not aws.csget("iam-role") do
    print("Waiting to establish IAM role...")
    sys.wait(1)
end
local _earthdata_auth_script = core.script("earth_data_auth"):name("NsidcAuthScript")
while not aws.csget("nsidc-cloud") do
    print("Waiting to authenticate to NSIDC...")
    sys.wait(1)
end

-- initialize parameters
local rqst = {
    ["asset"] = "icesat2",
    ["resource"] = "ATL03_20230213042035_08341807_006_02.h5",
    ["atl09_asset"] = "icesat2",
    ["use_bathy_mask"] = true,
    ["spots"] = {1, 2, 3, 4, 5, 6},
    ["classifiers"] = {"qtrees", "coastnet"},
    ["srt"] = -1,
    ["cnf"] = "atl03_not_considered",
    ["pass_invalid"] = true,
    ["timeout"] = 50000,
    ["correction"] = {
        ["use_water_ri_mask"] = true,
    },
    ["output"] = {
        ["path"] = "myfile.bin",
        ["format"] = "parquet",
        ["open_on_complete"] = false,
        ["as_geo"] = false,
        ["with_checksum"] = false
    }
}

local parms         = bathy.parms(rqst)
local resource      = parms["resource"]
local timeout       = parms["node_timeout"]
local rqsttime      = time.gps()
local outputs       = {} -- table of all outputs that go into atl24 writer
local profile       = {} -- timing profiling table
local rspq          = nil

-------------------------------------------------------
-- function: ctimeout
-------------------------------------------------------
local function ctimeout()
    local current_timeout = (timeout * 1000) - (time.gps() - rqsttime)
    if current_timeout < 0 then current_timeout = 0 end
    return math.tointeger(current_timeout)
end

-------------------------------------------------------
-- public check
-------------------------------------------------------
if sys.ispublic() then -- verify on a private cluster
    print(string.format("request <%s> forbidden on public cluster... exiting", rspq))
    return
end

-------------------------------------------------------
-- get Kd resource filename
-------------------------------------------------------
local rdate         = string.format("%04d-%02d-%02dT00:00:00Z", parms["year"], parms["month"], parms["day"])
local rgps          = time.gmt2gps(rdate)
local _,doy         = time.gps2gmt(rgps)
local doy_8d_start  = ((doy - 1) & ~7) + 1
local doy_8d_stop   = doy_8d_start + 7
local gps_start     = time.gmt2gps(string.format("%04d:%03d:00:00:00", parms["year"], doy_8d_start))
local gps_stop      = time.gmt2gps(string.format("%04d:%03d:00:00:00", parms["year"], doy_8d_stop))
local year_start, month_start, day_start = time.gps2date(gps_start)
local year_stop,  month_stop,  day_stop  = time.gps2date(gps_stop)
if year_start ~= year_stop then
    year_stop = year_start
    month_stop = 12
    day_stop = 31
end
local viirs_filename = string.format("JPSS1_VIIRS.%04d%02d%02d_%04d%02d%02d.L3m.8D.KD.Kd_490.4km.nc.dap.nc4",
    year_start, month_start, day_start,
    year_stop, month_stop, day_stop
)

-------------------------------------------------------
-- build time range (needed for NDWI and ATL09)
-------------------------------------------------------
local rdelta = 5 * 24 * 60 * 60 * 1000 -- 5 days * (24 hours/day * 60 minutes/hour * 60 seconds/minute * 1000 milliseconds/second)
local t0 = string.format('%04d-%02d-%02dT%02d:%02d:%02dZ', time.gps2date(rgps - rdelta))
local t1 = string.format('%04d-%02d-%02dT%02d:%02d:%02dZ', time.gps2date(rgps + rdelta))

-------------------------------------------------------
-- get ATL09 resources
-------------------------------------------------------
local resource09 = nil
local atl09_cmr_start_time = time.gps()
local atl09_parms = {
    asset = "icesat2-atl09",
    t0 = t0,
    t1 = t1,
    name_filter = '*_' .. string.format("%04d", parms["rgt"]) .. '????_*'
}
local atl09_max_retries = 3
local atl09_attempt = 1
while true do
    local rc2, rsps2 = earthdata.search(atl09_parms)
    if rc2 == earthdata.SUCCESS then
        if #rsps2 == 1 then
            resource09 = rsps2[1]
            break -- success
        else
            print(string.format("request <%s> returned an invalid number of resources for ATL09 CMR request for %s: %d", rspq, resource, #rsps2))
            return -- failure
        end
    else
        print(string.format("request <%s> failed attempt %d to make ATL09 CMR request <%d>: %s", rspq, atl09_attempt, rc2, rsps2))
        atl09_attempt = atl09_attempt + 1
        if atl09_attempt > atl09_max_retries then
            print(string.format("request <%s> failed to make ATL09 CMR request for %s... aborting!", rspq, resource))
            return -- failure
        end
    end
end
profile["atl09_cmr"] = (time.gps() - atl09_cmr_start_time) / 1000.0
print(string.format("ATL09 CMR search executed in %f seconds", profile["atl09_cmr"]))

-------------------------------------------------------
-- initialize container runtime environment
-------------------------------------------------------
local crenv = runner.setup()
if not crenv.host_sandbox_directory then
    print(string.format("failed to initialize container runtime for %s", resource))
    runner.cleanup(crenv)
    return
end

-------------------------------------------------------
-- create dataframe inputs
-------------------------------------------------------
local bathymask = bathy.mask()
local atl03h5 = h5.object(parms["asset"], resource)
local atl09h5 = h5.object(parms["asset09"], resource09)
local granule = (parms["output"]["format"] == "h5") and bathy.granule(parms, atl03h5, rspq) or nil
local kd490 = bathy.kd(parms, viirs_filename)
local refraction = bathy.refraction(parms)
local uncertainty = bathy.uncertainty(parms, kd490)
local seasurface = parms["find_sea_surface"] and bathy.seasurface(parms) or nil
local qtrees = parms:classifier(bathy.QTREES) and bathy.qtrees(parms) or nil
local coastnet = parms:classifier(bathy.QTREES) and bathy.coastnet(parms) or nil
local openoceanspp = parms:classifier(bathy.QTREES) and bathy.openoceanspp(parms) or nil

-------------------------------------------------------
-- build dataframes for each beam
-------------------------------------------------------
local dataframes = {}
for _, beam in ipairs(parms["beams"]) do
    dataframes[beam] = bathy.dataframe(beam, parms, bathymask, atl03h5, atl09h5, rspq)
    if not dataframes[beam] then
        print(string.format("request <%s> on %s failed to create bathy dataframe for beam %s", rspq, resource, beam))
    else
        dataframes[beam]:run(seasurface)
--        dataframes[beam]:run(qtrees)
--        dataframes[beam]:run(coastnet)
--        dataframes[beam]:run(openoceanspp)
--        dataframes[beam]:run(refraction)
--        dataframes[beam]:run(uncertainty)
        dataframes[beam]:run(core.TERMINATE)
    end
end

-------------------------------------------------------
-- wait for dataframes to complete and write to file
-------------------------------------------------------
for beam,dataframe in pairs(dataframes) do
    if dataframe:finished(ctimeout(), rspq) then
        if dataframes[beam]:length() <= 0 then
            print(string.format("request <%s> on %s created an empty bathy dataframe for spot %d", rspq, resource, dataframe:meta("spot")))
        elseif not dataframes[beam]:isvalid() then
            print(string.format("request <%s> on %s failed to create valid bathy dataframe for spot %d", rspq, resource, dataframe:meta("spot")))
            runner.cleanup(crenv)
            return
        else
            outputs[beam] = string.format("%s/%s_%d.parquet", crenv.host_sandbox_directory, bathy.BATHY_PREFIX, dataframe:meta("spot"))
            local arrow_dataframe = arrow.dataframe(parms, dataframe)
            if not arrow_dataframe then
                print(string.format("request <%s> on %s failed to create arrow dataframe for spot %d", rspq, resource, dataframe:meta("spot")))
                runner.cleanup(crenv)
                return
            elseif not arrow_dataframe:export(outputs[beam], arrow.PARQUET) then -- e.g. /share/bathy_spot_3.parquet)
                print(string.format("request <%s> on %s failed to write dataframe for spot %d", rspq, resource, dataframe:meta("spot")))
                runner.cleanup(crenv)
                return
            end
        end
    else
        print(string.format("request <%s> on %s timed out waiting for dataframe to complete on spot %d", rspq, resource, dataframe:meta("spot")))
        runner.cleanup(crenv)
        return
    end
end

-------------------------------------------------------
-- wait for granule to complete and write to file
-------------------------------------------------------
if granule then
    outputs["granule"] = string.format("%s/%s_granule.json", crenv.container_sandbox_mount, bathy.BATHY_PREFIX, "w")
    local f = io.open(outputs["granule"])
    if f then
        if granule:waiton(ctimeout(), rspq) then
            f:write(json.encode(granule:export()))
            f:close()
        else
            print(string.format("request <%s> timed out waiting for granule to complete on %s", rspq, resource))
            runner.cleanup(crenv)
            return
        end
    else
        print(string.format("request <%s> failed to write granule json for %s", rspq, resource))
        runner.cleanup(crenv)
        return
    end
end

-------------------------------------------------------
-- get profiles
-------------------------------------------------------
profile["seasurface"] = seasurface and seasurface:runtime() or 0.0
profile["refraction"] = refraction and refraction:runtime() or 0.0
profile["uncertainty"] = uncertainty and uncertainty:runtime() or 0.0
profile["qtrees"] = qtrees and qtrees:runtime() or 0.0
profile["coastnet"] = coastnet and coastnet:runtime() or 0.0
profile["openoceanspp"] = openoceanspp and openoceanspp:runtime() or 0.0

-- clean up object to cut down on memory usage
atl03h5:destroy()
atl09h5:destroy()
kd490:destroy()

-------------------------------------------------------
-- run oceaneyes
-------------------------------------------------------
local tmp_atl24_filename = "atl24.bin"
local container_parms = {
    image = "oceaneyes",
    name = "oceaneyes",
    command = string.format("/env/bin/python /runner.py %s/settings.json", crenv.container_sandbox_mount),
    timeout = ctimeout(),
    parms = {
        ["settings.json"] = {
            input_files = outputs,
            format = parms["output"]["format"],
            atl24_filename = crenv.container_sandbox_mount.."/"..tmp_atl24_filename,
            profile = profile,
            duration = (time.gps() - rqsttime) / 1000.0
        }
    }
}
local container = runner.execute(crenv, container_parms, rspq)
runner.wait(container, timeout)

-------------------------------------------------------
-- send final output to user
-------------------------------------------------------
arrow.send2user(crenv.host_sandbox_directory.."/"..tmp_atl24_filename, arrow.parms(parms["output"]), rspq)
arrow.send2user(crenv.host_sandbox_directory.."/"..tmp_atl24_filename..".json", arrow.parms(parms["output"]), rspq, parms["output"]["path"]..".json")

-------------------------------------------------------
-- exit 
-------------------------------------------------------
runner.cleanup(crenv)

