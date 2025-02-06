local runner = require("test_executive")
local console = require("console")

-- UdpSocket Unit Test --

local server = streaming.udp("127.0.0.1", 35505, streaming.SERVER):name("server")
local client = streaming.udp("127.0.0.1", 35505, streaming.CLIENT):name("client")

sys.wait(2)

local expected_message = "Hello World"
runner.assert(client:send(expected_message), "Failed to send message")
local actual_message = server:receive()

print("Message: ", actual_message)
runner.assert(expected_message == actual_message, "Failed to match messages")

server:close()
client:close()

-- Report Results --

runner.report()

