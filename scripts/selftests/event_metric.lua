local runner = require("test_executive")
local console = require("console")
local json = require("json")

-- Unit Test --

local endpoint = core.endpoint():name("metrictest"):metric() -- creates catchall
local server   = core.httpd(9081):attach(endpoint, "/source"):untilup()

local client = streaming.http("127.0.0.1", 9081)


print('\n------------------\nTest01: Catchall\n------------------')

local rsps = client:request("GET", "/source/version", "{}")
rsps = client:request("GET", "/source/version", "{}")
rsps = client:request("GET", "/source/version", "{}")

local metrics = sys.metric("metrictest")
local display = json.encode(metrics)

runner.check(metrics["catchall.hits"] == 3.0, "did not catch all requests")

print(display)

print('\n------------------\nTest02: Version\n------------------')

endpoint:metric("version")

rsps = client:request("GET", "/source/version", "{}")
rsps = client:request("GET", "/source/version", "{}")

metrics = sys.metric("metrictest")
display = json.encode(metrics)

runner.check(metrics["catchall.hits"] == 3.0, "mischaracterized requests")
runner.check(metrics["version.hits"] == 2.0, "did not associate requests")

print(display)

-- Clean Up --

server:destroy()
client:destroy()

-- Report Results --

runner.report()

