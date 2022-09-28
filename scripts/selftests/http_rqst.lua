local runner = require("test_executive")

-- Setup --

endpoint = core.endpoint()
server   = core.httpd(9081):attach(endpoint, "/source"):name("server"):untilup()
client = core.http("127.0.0.1", 9081)

-- Unit Test --

rsps, code, status = client:request("GET", "/source/health", "{}")
runner.check(status == true, "request failed")

-- Clean Up --

server:destroy()
client:destroy()

-- Report Results --

runner.report()

