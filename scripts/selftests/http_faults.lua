local runner = require("test_executive")
local console = require("console")
local json = require("json")

-- Setup --

endpoint = core.endpoint()
server   = core.httpd(9081):attach(endpoint, "/source")

client = core.http("127.0.0.1", 9081)

-- Unit Test --

rsps = client:request("GET", "/", "{}")
runner.check(rsps == nil, "failed to report error on unattached endpoint")

rsps = client:request("RAW", "/", "\r\n\r\n")
runner.check(rsps == nil, "failed to report error on empty http request")

rsps, status = client:request("GET", "/source/health", "{}")
runner.check(status == true, "failed to remain healthy after invalid requests")

-- Clean Up --

server:destroy()
client:destroy()

-- Report Results --

runner.report()

