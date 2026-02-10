local runner = require("test_executive")

-- Setup --

local endpoint = core.endpoint()
local server   = core.httpd(10081):attach(endpoint, "/source"):untilup()

-- Self Test --

runner.unittest("HTTP Faults (not found)", function()
    local client = streaming.http("127.0.0.1", 10081)
    local rsps, code, status = client:request("GET", "/", "{}")
    runner.assert(status == nil, "failed to report error on unattached endpoint")
    client:destroy()
end)

runner.unittest("HTTP Faults (empty)", function()
    local client = streaming.http("127.0.0.1", 10081)
    local rsps, code, status = client:request("RAW", "/", "\r\n\r\n")
    runner.assert(status == nil, "failed to report error on empty http request")
    client:destroy()
end)

runner.unittest("HTTP Faults (health)", function()
    local client = streaming.http("127.0.0.1", 10081)
    local rsps, code, status = client:request("GET", "/source/health", "{}")
    runner.assert(status == true, "failed to remain healthy after invalid requests")
    client:destroy()
end)

-- Clean Up --

server:destroy()

-- Report Results --

runner.report()
