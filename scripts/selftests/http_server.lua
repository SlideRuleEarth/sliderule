local runner = require("test_executive")
local console = require("console")

-- Setup --

json_object = '{ \
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

-- Unit Test --

local endpoint = core.endpoint()
local server   = core.httpd(9081):attach(endpoint, "/source"):untilup()

print('\n------------------\nTest01: Stream\n------------------')
os.execute(string.format("curl -sS -X POST -d '%s' http://127.0.0.1:9081/source/example_engine_endpoint > %s", json_object, tmpfile))
local f = io.open(tmpfile)
local result1 = f:read()
local result2 = f:read()
local result3 = f:read()
f:close()
runner.check(result1 == "FILE", "result1="..result1)
runner.check(result2 == "P01_01.dat", "result2="..result2)
runner.check(result3 == "CCSDS", "result3="..result3)

print('\n------------------\nTest02: Return\n------------------')
os.execute(string.format("curl -sS -X GET -d '%s' http://127.0.0.1:9081/source/example_source_endpoint > %s", json_object, tmpfile))
f = io.open(tmpfile)
local result = f:read()
f:close()
runner.check(result == "{ \"result\": \"Hello World\" }")

-- Clean Up --

server:destroy()
os.remove(tmpfile)

-- Report Results --

runner.report()

