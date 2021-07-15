local runner = require("test_executive")
local console = require("console")
local json = require("json")

-- Setup --

server = core.httpd(9081)
endpoint = core.endpoint()
server:attach(endpoint, "/source")

client = core.http("127.0.0.1", 9081)

-- Unit Test --

rsps, status = client:request("GET", "/", "{}")
print(rsps, status)
runner.check(status == nil, "failed to report error on unattached endpoint")

rsps, status = client:request("RAW", "/", "\r\n\r\n")
print(rsps, status)
runner.check(status == nil, "failed to report error on empty http request")

rsps, status = client:request("GET", "/source/health", "{}")
print(rsps, status)
runner.check(status == true, "failed to remain healthy after invalid requests")

-- Clean Up --

server:destroy()
client:destroy()

-- Report Results --

runner.report()

