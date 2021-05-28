local runner = require("test_executive")
local console = require("console")
local json = require("json")

-- Unit Test --

server = core.httpd(9081)
endpoint = core.endpoint()
server:attach(endpoint, "/source")

client = core.http("127.0.0.1", 9081)

rsps = client:request("GET", "/source/version", "{}")
rsps = client:request("GET", "/source/version", "{}")
rsps = client:request("GET", "/source/version", "{}")

metrics = sys.metric("LuaEndpoint")
display = json.encode(metrics)

print(display)

-- Clean Up --

server:destroy()
client:destroy()

-- Report Results --

runner.report()

