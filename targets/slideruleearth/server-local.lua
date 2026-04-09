--------------------------------------------------
-- Minimal Local Server (no AWS dependencies)
--------------------------------------------------

-- Configure Application Endpoints --
local source_endpoint = core.endpoint():global("SourceEndpoint")
local arrow_endpoint = arrow.endpoint():global("ArrowEndpoint")

-- Run Application HTTP Server --
local app_server = core.httpd(9081):global("AppServer")
app_server:attach(source_endpoint, "/source")
app_server:attach(arrow_endpoint, "/arrow")
