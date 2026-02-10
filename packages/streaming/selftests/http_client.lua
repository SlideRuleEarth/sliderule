local runner = require("test_executive")
local json = require("json")

-- Setup --

local endpoint  = core.endpoint()
local server    = core.httpd(10081):attach(endpoint, "/source"):untilup()
local client    = streaming.http("127.0.0.1", 10081)

-- Self Test --

runner.unittest("HTTP Client (version)", function()
    local rsps, code, status = client:request("GET", "/source/version", "{}")
    local rsps_table = json.decode(rsps)
    runner.assert(rsps_table["server"]["packages"][1] == "msg")
    runner.assert(code == 200)
    runner.assert(status)
end)

-- Self Test --

runner.unittest("HTTP Client (example source endpoint)", function()
    local json_object = '{ \
        \"var1\": false, \
        \"var2\": \"*.rec\", \
        \"var3\": 4, \
        \"var4\": { \
            \"type\": \"FILE\", \
            \"files\": \"P01_01.dat\", \
            \"format\": \"CCSDS\" \
        } \
    }'
    local rsps = client:request("GET", "/source/example_source_endpoint", json_object)
    local rsps_table = json.decode(rsps)
    runner.assert(rsps_table["result"] == "Hello World")
end)

-- Self Test --

runner.unittest("HTTP Client (health)", function()
    local rsps = client:request("GET", "/source/health", "{}")
    local rsps_table = json.decode(rsps)
    runner.assert(rsps_table["healthy"] == true)
end)

-- Clean Up --

server:destroy()
client:destroy()

-- Report Results --

runner.report()
