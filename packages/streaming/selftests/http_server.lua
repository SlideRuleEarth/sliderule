local runner = require("test_executive")

-- Setup --

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

local tmpfile = os.tmpname()

local endpoint = core.endpoint()
local server   = core.httpd(10081):attach(endpoint, "/source"):untilup()

-- Self Test --

runner.unittest("HTTP Server: Stream", function()
    os.execute(string.format("curl -sS -X POST -d '%s' http://127.0.0.1:10081/source/example_engine_endpoint > %s", json_object, tmpfile))
    local f = assert(io.open(tmpfile))
    local result1 = f:read()
    local result2 = f:read()
    local result3 = f:read()
    f:close()
    runner.assert(result1 == "FILE", "result1="..result1)
    runner.assert(result2 == "P01_01.dat", "result2="..result2)
    runner.assert(result3 == "CCSDS", "result3="..result3)
end)

runner.unittest("HTTP Server: Normal", function()
    os.execute(string.format("curl -sS -X GET -d '%s' http://127.0.0.1:10081/source/example_source_endpoint > %s", json_object, tmpfile))
    local f = assert(io.open(tmpfile))
    local result = f:read()
    f:close()
    runner.assert(result == "{ \"result\": \"Hello World\" }")
end)

-- Clean Up --

server:destroy()
os.remove(tmpfile)

-- Report Results --

runner.report()
