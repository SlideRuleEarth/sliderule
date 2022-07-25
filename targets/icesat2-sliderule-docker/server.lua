local global = require("global")
local asset = require("asset")
local json = require("json")

-- Function to return all available system scripts
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

-- Process Arguments: JSON Configuration File
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

-- Pull Out Parameters --
local event_format = global.eval(cfgtbl["event_format"]) or core.FMT_TEXT
local event_level = global.eval(cfgtbl["event_level"]) or core.INFO
local port = cfgtbl["server_port"] or 9081
local authenticate_to_earthdata = cfgtbl["authenticate_to_earthdata"] or false
local asset_directory = cfgtbl["asset_directory"] or nil
local normal_mem_thresh = cfgtbl["normal_mem_thresh"] or 1.0
local stream_mem_thresh = cfgtbl["stream_mem_thresh"] or 0.75
local msgq_depth = cfgtbl["msgq_depth"] or 10000

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

-- Run Earth Data Authentication Script --
if authenticate_to_earthdata then
    local auth_script = core.script("earth_data_auth", "")
    auth_script:name("auth_script")
end

-- Run Service Registry Script --
local service_script = core.script("service_registry")
service_script:name("service_script")

-- Configure Endpoints --
local source_endpoint = core.endpoint(normal_mem_thresh, stream_mem_thresh)
source_endpoint:name("SourceEndpoint")
local probe_endpoint = core.endpoint(1.0, 1.0, core.DEBUG)
probe_endpoint:name("ProbeEndpoint")
for _,script in ipairs(available_scripts()) do
    local s = script:find(".lua")
    if s then
        local metric_name = script:sub(0,s-1)
        source_endpoint:metric(metric_name)
        probe_endpoint:metric(metric_name)
    end
end

-- Run Server --
local server = core.httpd(port)
server:name("HttpServer")
server:metric() -- register server metrics
server:attach(source_endpoint, "/source")
server:attach(probe_endpoint, "/probe")
