local global = require("global")
local asset = require("asset")
local json = require("json")

--------------------------------------------------
-- Functions
--------------------------------------------------

-- Returns All Available Endpoint Scripts --
local function available_scripts()
    local i = 0
    local scripts = {}
    local pdir = io.popen('ls "' .. __confdir .. '/api"')
    if pdir ~= nil then
        for filename in pdir:lines() do
            i = i + 1
            scripts[i] = filename
        end
        pdir:close()
    end
    return scripts
end

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
local authenticate_to_earthdata = cfgtbl["authenticate_to_earthdata"] -- nil is false
local register_as_service       = cfgtbl["register_as_service"] -- nil is false
local asset_directory           = cfgtbl["asset_directory"] or __confdir.."/asset_directory.csv"
local normal_mem_thresh         = cfgtbl["normal_mem_thresh"] or 1.0
local stream_mem_thresh         = cfgtbl["stream_mem_thresh"] or 0.75
local msgq_depth                = cfgtbl["msgq_depth"] or 10000
local orchestrator_url          = cfgtbl["orchestrator"] or os.getenv("ORCHESTRATOR")
local org_name                  = cfgtbl["cluster"] or os.getenv("CLUSTER")
local ps_url                    = cfgtbl["provisioning_system"] or os.getenv("PROVISIONING_SYSTEM")
local ps_auth                   = cfgtbl["authenticate_to_ps"] -- nil is false

--------------------------------------------------
-- System Configuration
--------------------------------------------------

-- Configure System Message Queue Depth --
sys.setstddepth(msgq_depth)

-- Configure Monitoring --
sys.setlvl(core.LOG | core.TRACE | core.METRIC, event_level) -- set level globally
local monitor = core.monitor(core.LOG, event_level, event_format):name("EventMonitor") -- monitor only logs
monitor:tail(1024)

local dispatcher = core.dispatcher(core.EVENTQ):name("EventDispatcher")
dispatcher:attach(monitor, "eventrec")
dispatcher:run()

-- Configure Assets --
local assets = asset.loaddir(asset_directory, true)

-- Run IAM Role Authentication Script -
local role_auth_script = core.script("iam_role_auth"):name("RoleAuthScript")

-- Run Earth Data Authentication Script --
if authenticate_to_earthdata then
    local earthdata_auth_script = core.script("earth_data_auth", ""):name("EarthdataAuthScript")
end

-- Initialize Orchestrator --
netsvc.orchurl(orchestrator_url)
if register_as_service then
    local service_script = core.script("service_registry", "http://"..sys.ipv4()..":"..tostring(app_port)):name("ServiceScript")
end

--------------------------------------------------
-- Application Server
--------------------------------------------------

-- Configure Application Endpoints --
local source_endpoint = core.endpoint(normal_mem_thresh, stream_mem_thresh):name("SourceEndpoint")
for _,script in ipairs(available_scripts()) do
    local s = script:find(".lua")
    if s then
        local metric_name = script:sub(0,s-1)
        source_endpoint:metric(metric_name)
    end
end

-- Configure Provisioning System Authentication --
netsvc.psurl(ps_url)
netsvc.psorg(org_name)
if ps_auth then
    local authenticator = netsvc.psauth()
    source_endpoint:auth(authenticator)
end

-- Run Application HTTP Server --
local app_server = core.httpd(app_port):name("AppServer")
app_server:metric() -- register server metrics
app_server:attach(source_endpoint, "/source")

--------------------------------------------------
-- Probe Server (internal)
--------------------------------------------------

-- Configure Probe Endpoints --
local probe_endpoint = core.endpoint(1.0, 1.0, core.DEBUG):name("ProbeEndpoint")

-- Run Probe HTTP Server --
local probe_server = core.httpd(probe_port):name("ProbeServer")
probe_server:attach(probe_endpoint, "/probe")
