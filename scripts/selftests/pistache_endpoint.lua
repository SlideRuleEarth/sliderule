local runner = require("test_executive")
local console = require("console")

-- Setup --

parm = "hello=world"

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

local server = pistache.server(9082):name("myengine")

sys.wait(1)

print('\n------------------\nTest01: Echo\n------------------')
os.execute(string.format("curl -sS -d \"%s\" http://127.0.0.1:9082/echo -o %s", parm, tmpfile))
local f = io.open(tmpfile)
if f ~= nil then
    result = f:read()
    f:close()
    runner.check(result == parm)
else
    runner.check(false, "failed to open file")
end

print('\n------------------\nTest02: Engine\n------------------')
os.execute(string.format("curl -sS -X POST -d '%s' http://127.0.0.1:9082/source/example_engine_endpoint > %s", json_object, tmpfile))
f = io.open(tmpfile)
if f ~= nil then
    f:read(4)
    local result1 = f:read()
    f:read(4)
    local result2 = f:read()
    f:read(4)
    local result3 = f:read()
    f:close()
    runner.check(result1 == "FILE")
    runner.check(result2 == "P01_01.dat")
    runner.check(result3 == "CCSDS")
else
    runner.check(false, "failed to open file")
end

print('\n------------------\nTest03: Source\n------------------')
os.execute(string.format("curl -sS -X GET -d '%s' http://127.0.0.1:9082/source/example_source_endpoint > %s", json_object, tmpfile))
f = io.open(tmpfile)
if f ~= nil then
    local result = f:read()
    f:close()
    runner.check(result == "{ \"result\": \"Hello World\" }")
else
    runner.check(false, "failed to open file")
end

-- Clean Up --

server:destroy()
os.remove(tmpfile)

-- Report Results --

runner.report()

