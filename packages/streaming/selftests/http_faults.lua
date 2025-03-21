local runner = require("test_executive")

-- Setup --

local endpoint = core.endpoint()
local server   = core.httpd(9081):attach(endpoint, "/source"):untilup()

-- Self Test --

local client = streaming.http("127.0.0.1", 9081)
local rsps, code, status = client:request("GET", "/", "{}")
runner.assert(status == nil, "failed to report error on unattached endpoint")
client:destroy()

client = streaming.http("127.0.0.1", 9081)
rsps, code, status = client:request("RAW", "/", "\r\n\r\n")
runner.assert(status == nil, "failed to report error on empty http request")
client:destroy()

client = streaming.http("127.0.0.1", 9081)
rsps, code, status = client:request("GET", "/source/health", "{}")
runner.assert(status == true, "failed to remain healthy after invalid requests")

-- Clean Up --

client:destroy()
server:destroy()

-- Report Results --

runner.report()
