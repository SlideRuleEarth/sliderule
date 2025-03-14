local runner = require("test_executive")
local console = require("console")
local json = require("json")

-- Unit Test --

local endpoint = core.endpoint()
local server   = core.httpd(9081):attach(endpoint, "/source"):untilup()

local client = streaming.http("127.0.0.1", 9081)

print('\n------------------\nTest01: Return\n------------------')

local rsps, code, status = client:request("GET", "/source/version", "{}")
local rsps_table = json.decode(rsps)
runner.assert(rsps_table["server"]["packages"][1] == "core")
runner.assert(code == 200)
runner.assert(status)

print('\n------------------\nTest02: Return\n------------------')

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

rsps = client:request("GET", "/source/example_source_endpoint", json_object)
print(rsps)
rsps_table = json.decode(rsps)
runner.assert(rsps_table["result"] == "Hello World")

print('\n------------------\nTest03: Return\n------------------')

rsps = client:request("GET", "/source/health", "{}")
print(rsps)
rsps_table = json.decode(rsps)
runner.assert(rsps_table["healthy"] == true)

-- Clean Up --

server:destroy()
client:destroy()

-- Report Results --

runner.report()
