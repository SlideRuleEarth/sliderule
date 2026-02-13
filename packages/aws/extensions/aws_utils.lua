local json = require("json")
local earthdata = require("earth_data_query")

-- Configure In Cloud --
local function config_aws ()
    local status = false
    local http_code = core.servicecheck("http://169.254.169.254/latest/meta-data/", 1)
    if (http_code >= 200 and http_code < 300) or (http_code == 401) then
        print("Executing in the cloud: "..tostring(http_code))
        status = true
    else
        print("Executing locally: "..tostring(http_code))
    end
    sys.setcfg("in_cloud", status)
    return status
end

-- Configure Monitoring --
local function config_monitoring ()
    core.logmon(core.DEBUG):global("LogMonitor") -- monitor logs and write to stdout
    local alert_stream = sys.getcfg("alert_stream")
    if #alert_stream > 0 then
        aws.firehose(core.DEBUG, core.ALERT_REC_TYPE, alert_stream):global("AlertMonitor") -- monitor alerts and push to firehose
    end
    local telemetry_stream = sys.getcfg("telemetry_stream")
    if #telemetry_stream > 0 then
        aws.firehose(core.DEBUG, core.TLM_REC_TYPE, telemetry_stream):global("TelemetryMonitor") -- monitor telementry and push to firehose
    end
end

-- Update Leap Seconds File --
local function config_leap_seconds ()
    local leap_seconds_file = "/tmp/leap-seconds.list"
    local leap_seconds_service = "https://data.iana.org/time-zones/tzdb/leap-seconds.list"
    core.download(leap_seconds_service, leap_seconds_file)
    if sys.upleap(leap_seconds_file) then
        sys.log(core.CRITICAL, "Successfully updated leap seconds from "..leap_seconds_service)
    end
end

-- Configure Earth Data Assets and Credentials --
local function config_earth_data ()
    -- load earthdata assets
    earthdata.load()
    -- run IAM role authentication script (identity="iam-role")
    core.script("iam_role_auth"):global("RoleAuthScript")
    local iam_role_max_wait = 10
    while not aws.csget("iam-role") do
        iam_role_max_wait = iam_role_max_wait - 1
        if iam_role_max_wait == 0 then
            sys.log(core.CRITICAL, "Failed to establish IAM role credentials at startup")
            break
        else
            sys.log(core.CRITICAL, "Waiting to establish IAM role...")
            sys.wait(1)
        end
    end
    sys.log(core.CRITICAL, "IAM role established")
    -- run earth data authentication scripts
    if sys.getcfg("authenticate_to_nsidc") then
        local script_parms = {earthdata="https://data.nsidc.earthdatacloud.nasa.gov/s3credentials", identity="nsidc-cloud"}
        core.script("earth_data_auth", json.encode(script_parms)):global("NsidcAuthScript")
    end
    if sys.getcfg("authenticate_to_ornldaac") then
        local script_parms = {earthdata="https://data.ornldaac.earthdata.nasa.gov/s3credentials", identity="ornl-cloud"}
        core.script("earth_data_auth", json.encode(script_parms)):global("OrnldaacAuthScript")
    end
    if sys.getcfg("authenticate_to_lpdaac") then
        local script_parms = {earthdata="https://data.lpdaac.earthdatacloud.nasa.gov/s3credentials", identity="lpdaac-cloud"}
        core.script("earth_data_auth", json.encode(script_parms)):global("LpdaacAuthScript")
    end
    if sys.getcfg("authenticate_to_podaac") then
        local script_parms = {earthdata="https://archive.podaac.earthdata.nasa.gov/s3credentials", identity="podaac-cloud"}
        core.script("earth_data_auth", json.encode(script_parms)):global("PodaacAuthScript")
    end
    if sys.getcfg("authenticate_to_asf") then
        local script_parms = {earthdata="https://nisar.asf.earthdatacloud.nasa.gov/s3credentials", identity="asf-cloud"}
        core.script("earth_data_auth", json.encode(script_parms)):global("AsfAuthScript")
    end
end

-- Exported Package --
return {
    config_aws = config_aws,
    config_monitoring = config_monitoring,
    config_leap_seconds = config_leap_seconds,
    config_earth_data = config_earth_data
}
