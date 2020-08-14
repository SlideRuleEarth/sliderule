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

tmpfile = os.tmpname()

console.logger:config(core.INFO)

-- Unit Test --

server = core.httpd(9081)
endpoint = core.endpoint("/source")
server:attach(endpoint)

sys.wait(1)

print('\n------------------\nTest01: Echo\n------------------')
os.execute(string.format("curl -sS -X GET -d \"%s\" http://127.0.0.1:9081/source/time", parm))

--[[
print('\n------------------\nTest02: Engine\n------------------')
os.execute(string.format("curl -sS -d '%s' http://127.0.0.1:9081/engine/example_engine_endpoint > %s", json_object, tmpfile))
f = io.open(tmpfile)
f:read(4)
result1 = f:read()
f:read(4)
result2 = f:read()
f:read(4)
result3 = f:read()
f:close()
runner.check(result1 == "FILE")
runner.check(result2 == "P01_01.dat")
runner.check(result3 == "CCSDS")

print('\n------------------\nTest03: Source\n------------------')
os.execute(string.format("curl -sS -d '%s' http://127.0.0.1:9081/source/example_source_endpoint > %s", json_object, tmpfile))
f = io.open(tmpfile)
result = f:read()
f:close()
runner.check(result == "{ \"result\": \"Hello World\" }")

-- Clean Up --

os.remove(tmpfile)
--]]
-- Report Results --

runner.report()

