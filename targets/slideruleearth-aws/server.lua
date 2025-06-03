local asset = require("asset")
local json = require("json")
local aws_utils = require("aws_utils")

--------------------------------------------------
-- System Configuration
--------------------------------------------------

-- Read JSON Configuration File --
local cfgtbl = {}
local json_input = arg[1]
if json_input and string.match(json_input, ".json") then
    sys.log(core.INFO, string.format('Reading json file: %s', json_input))
    local f = io.open(json_input, "r")
    if f ~= nil then
        local content = f:read("*all")
        f:close()
        cfgtbl = json.decode(content)
    end
end

-- Populate System Configuration --
sys.initcfg(cfgtbl)

-- Configure In Cloud --
aws_utils.config_aws()

-- Configure Monitoring --
core.logmon(core.DEBUG):global("LogMonitor") -- monitor logs and write to stdout
core.tlmmon(core.DEBUG):global("TelemetryMonitor") -- monitor telementry and push to orchestrator and manager
core.alrmon(core.DEBUG):global("AlertMonitor") -- monitor alerts and push to manager

-- Configure Assets --
asset.loaddir(sys.getcfg("asset_directory"))

-- Run IAM Role Authentication Script (identity="iam-role") --
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

-- Run Earth Data Authentication Scripts --
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

--------------------------------------------------
-- Application Server
--------------------------------------------------

-- Configure Application Endpoints --
local source_endpoint = core.endpoint():global("SourceEndpoint")
local arrow_endpoint = arrow.endpoint():global("ArrowEndpoint")

-- Configure Provisioning System Authentication --
if sys.getcfg("authenticate_to_prov_sys") then
    local authenticator = core.psauth()
    source_endpoint:auth(authenticator)
    arrow_endpoint:auth(authenticator)
end

-- Run Application HTTP Server --
local app_server = core.httpd(sys.getcfg("app_port")):global("AppServer")
app_server:attach(source_endpoint, "/source")
app_server:attach(arrow_endpoint, "/arrow")

--------------------------------------------------
-- Register Services
--------------------------------------------------

-- Initialize Orchestrator --
if sys.getcfg("register_as_service") then
    local service_url = "http://"..sys.getcfg("ipv4")..":"..tostring(sys.getcfg("app_port"))
    core.script("service_registry", service_url):global("ServiceScript")
end

--------------------------------------------------
-- Post Startup
--------------------------------------------------

-- Scripts --
for _,script in ipairs(sys.getcfg("post_startup_scripts")) do
    core.script(script)
end