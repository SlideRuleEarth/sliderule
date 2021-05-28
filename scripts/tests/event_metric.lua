local runner = require("test_executive")
local console = require("console")
local json = require("json")

-- Unit Test --

server = core.httpd(9081)
endpoint = core.endpoint()
endpoint:name("metrictest")
endpoint:metric() -- creates catchall
server:attach(endpoint, "/source")

client = core.http("127.0.0.1", 9081)


print('\n------------------\nTest01: Catchall\n------------------')

rsps = client:request("GET", "/source/version", "{}")
rsps = client:request("GET", "/source/version", "{}")
rsps = client:request("GET", "/source/version", "{}")

metrics = sys.metric("metrictest")
display = json.encode(metrics)

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

