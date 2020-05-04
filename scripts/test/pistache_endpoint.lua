local runner = require("test_executive")
local console = require("console")

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

console.logger:config(core.INFO)

-- LuaEndpoint Unit Test --

server = pistache.endpoint(9081)
server:name("myengine")

sys.wait(1)

print('\n------------------\nTest01: Echo\n------------------')
os.execute("curl -d hello=world http://127.0.0.01:9081/echo")
print('')

print('\n------------------\nTest02: Engine\n------------------')
os.execute(string.format("curl -d '%s' http://127.0.0.01:9081/engine/example_engine_endpoint", json_object))

print('\n------------------\nTest03: Source\n------------------')
os.execute(string.format("curl -d '%s' http://127.0.0.01:9081/source/example_source_endpoint", json_object))


-- Report Results --

runner.report()

