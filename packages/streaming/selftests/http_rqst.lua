local runner = require("test_executive")

-- Setup --

local endpoint = core.endpoint()
local server   = core.httpd(9081):attach(endpoint, "/source"):untilup()
local client = streaming.http("127.0.0.1", 9081)

-- Self Test --

local rsps, code, status = client:request("GET", "/source/health", "{}")
runner.assert(status == true, "request failed")

-- Clean Up --

server:destroy()
client:destroy()

-- Report Results --

runner.report()

