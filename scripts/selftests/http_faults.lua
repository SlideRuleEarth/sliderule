local runner = require("test_executive")
local console = require("console")
local json = require("json")

-- Setup --

local endpoint = core.endpoint()
local server   = core.httpd(9081):attach(endpoint, "/source"):untilup()

-- Unit Test --

local client = streaming.http("127.0.0.1", 9081)
local rsps, code, status = client:request("GET", "/", "{}")
runner.check(status == nil, "failed to report error on unattached endpoint")
client:destroy()

client = streaming.http("127.0.0.1", 9081)
rsps, code, status = client:request("RAW", "/", "\r\n\r\n")
runner.check(status == nil, "failed to report error on empty http request")
client:destroy()

client = streaming.http("127.0.0.1", 9081)
rsps, code, status = client:request("GET", "/source/health", "{}")
runner.check(status == true, "failed to remain healthy after invalid requests")
client:destroy()

-- Clean Up --

server:destroy()

-- Report Results --

runner.report()

