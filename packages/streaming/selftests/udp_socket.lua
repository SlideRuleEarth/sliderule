local runner = require("test_executive")

-- Self Test --

local server = streaming.udp("127.0.0.1", 35505, streaming.SERVER)
local client = streaming.udp("127.0.0.1", 35505, streaming.CLIENT)

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
