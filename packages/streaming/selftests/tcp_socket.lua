local runner = require("test_executive")
local console = require("console")

-- Self Test --

local server = streaming.tcp("127.0.0.1", 35505, streaming.SERVER)
local client = streaming.tcp("127.0.0.1", 35505, streaming.CLIENT)

sys.wait(2)

local expected_message = "Hello World"
runner.assert(client:send(expected_message), "Failed to send message")
local actual_message = server:receive()

print("Message: ", actual_message)
runner.assert(expected_message == actual_message, "Failed to match messages")

-- Clean Up --

server:close()
client:close()

-- Report Results --

runner.report()
