local global = require("global")
local asset = require("asset")
local json = require("json")

--------------------------------------------------
-- Process Arguments
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

-- Set Parameters --
local event_format              = global.eval(cfgtbl["event_format"]) or core.FMT_TEXT
local event_level               = global.eval(cfgtbl["event_level"]) or core.INFO
local app_port                  = cfgtbl["app_port"] or 9081
local probe_port                = cfgtbl["probe_port"] or 10081
local authenticate_to_nsidc     = cfgtbl["authenticate_to_nsidc"] -- nil is false
local authenticate_to_ornldaac  = cfgtbl["authenticate_to_ornldaac"] -- nil is false
local authenticate_to_lpdaac    = cfgtbl["authenticate_to_lpdaac"] -- nil is false
local authenticate_to_podaac    = cfgtbl["authenticate_to_podaac"] -- nil is false
local register_as_service       = cfgtbl["register_as_service"] -- nil is false
local asset_directory           = cfgtbl["asset_directory"]
local normal_mem_thresh         = cfgtbl["normal_mem_thresh"] or 1.0
local stream_mem_thresh         = cfgtbl["stream_mem_thresh"] or 0.75
local msgq_depth                = cfgtbl["msgq_depth"] or 10000
local environment_version       = cfgtbl["environment_version"] or os.getenv("ENVIRONMENT_VERSION") or "unknown"
local orchestrator_url          = cfgtbl["orchestrator"] or os.getenv("ORCHESTRATOR")
local org_name                  = cfgtbl["cluster"] or os.getenv("CLUSTER")
local ps_url                    = cfgtbl["provisioning_system"] or os.getenv("PROVISIONING_SYSTEM")
local ps_auth                   = cfgtbl["authenticate_to_ps"] -- nil is false
local container_registry        = cfgtbl["container_registry"] or os.getenv("CONTAINER_REGISTRY")
local is_public                 = cfgtbl["is_public"] or os.getenv("IS_PUBLIC") or "False"

--------------------------------------------------
-- System Configuration
--------------------------------------------------

-- Set Environment Version --
sys.setenvver(environment_version)

-- Set Is Public --
sys.setispublic(is_public)

-- Set Cluster Name --
sys.setcluster(org_name)

-- Configure System Message Queue Depth --
sys.setstddepth(msgq_depth)

-- Configure Memory Limit --
sys.setmemlimit(stream_mem_thresh)

-- Configure Monitoring --
sys.setlvl(core.LOG | core.TRACE | core.METRIC, event_level) -- set level globally
local log_monitor = core.monitor(core.LOG, core.DEBUG, event_format):name("LogMonitor") -- monitor logs and write to stdout
log_monitor:tail(1024)
local metric_monitor = netsvc.mmonitor(core.DEBUG):name("MetricMonitor") -- monitor metrics and push to orchestrator
local dispatcher = core.dispatcher(core.EVENTQ, 1):name("EventDispatcher")
dispatcher:attach(log_monitor, "eventrec")
dispatcher:attach(metric_monitor, "eventrec")
dispatcher:run()

-- Configure Assets --
local assets = asset.loaddir(asset_directory)

-- Run IAM Role Authentication Script (identity="iam-role") --
local role_auth_script = core.script("iam_role_auth"):name("RoleAuthScript")
local iam_role_max_wait = 10
while not aws.csget("iam-role") do
    iam_role_max_wait = iam_role_max_wait - 1
    if iam_role_max_wait == 0 then
        print("Failed to establish IAM role credentials at startup")
        break
    else
        print("Waiting to establish IAM role...")
        sys.wait(1)
    end
end

-- Run Earth Data Authentication Scripts --
if authenticate_to_nsidc then
    local script_parms = {earthdata="https://data.nsidc.earthdatacloud.nasa.gov/s3credentials", identity="nsidc-cloud"}
    local earthdata_auth_script = core.script("earth_data_auth"):name("NsidcAuthScript")
end
if authenticate_to_ornldaac then
    local script_parms = {earthdata="https://data.ornldaac.earthdata.nasa.gov/s3credentials", identity="ornl-cloud"}
    local earthdata_auth_script = core.script("earth_data_auth", json.encode(script_parms)):name("OrnldaacAuthScript")
end
if authenticate_to_lpdaac then
    local script_parms = {earthdata="https://data.lpdaac.earthdatacloud.nasa.gov/s3credentials", identity="lpdaac-cloud"}
    local earthdata_auth_script = core.script("earth_data_auth", json.encode(script_parms)):name("LpdaacAuthScript")
end
if authenticate_to_podaac then
    local script_parms = {earthdata="https://archive.podaac.earthdata.nasa.gov/s3credentials", identity="podaac-cloud"}
    local earthdata_auth_script = core.script("earth_data_auth", json.encode(script_parms)):name("PodaacAuthScript")
end

-- Configure Container Registry -- 
if __cre__ then
   cre.setregistry(container_registry) 
end

--------------------------------------------------
-- Application Server
--------------------------------------------------

-- Configure Application Endpoints --
local source_endpoint = core.endpoint(normal_mem_thresh, stream_mem_thresh):name("SourceEndpoint")

-- Configure Provisioning System Authentication --
netsvc.psurl(ps_url)
netsvc.psorg(org_name)
if ps_auth then
    local authenticator = netsvc.psauth()
    source_endpoint:auth(authenticator)
end

-- Run Application HTTP Server --
local app_server = core.httpd(app_port):name("AppServer")
app_server:attach(source_endpoint, "/source")

--------------------------------------------------
-- Register Service
--------------------------------------------------

-- Initialize Orchestrator --
netsvc.orchurl(orchestrator_url)
if register_as_service then
    local service_script = core.script("service_registry", "http://"..sys.ipv4()..":"..tostring(app_port)):name("ServiceScript")
end
