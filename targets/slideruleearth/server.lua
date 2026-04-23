local aws_utils = require("aws_utils")

--------------------------------------------------
-- System Configuration
--------------------------------------------------

aws_utils.config_aws() -- in cloud
aws_utils.config_monitoring() -- logs (stdout, firehose)
aws_utils.config_leap_seconds() -- leap seconds
aws_utils.config_earth_data() -- assets and credentials

--------------------------------------------------
-- Application Server
--------------------------------------------------

-- Configure Application Endpoints --
local source_endpoint = core.endpoint():global("SourceEndpoint")

-- Run Application HTTP Server --
local app_server = core.httpd({
    ["/source"] = source_endpoint
}, sys.getcfg("app_port")):global("AppServer")

--------------------------------------------------
-- Register Services
--------------------------------------------------

-- Initialize Orchestrator --
if sys.getcfg("register_as_service") then
    local service_url = "http://"..sys.getcfg("ipv4")..":"..tostring(sys.getcfg("app_port"))
    core.script("service_registry", service_url):global("ServiceScript")
end
