local runner = require("test_executive")
local console = require("console")

-- TcpSocket Unit Test --

local server = streaming.tcp("127.0.0.1", 35505, streaming.SERVER):name("server") 
local client = streaming.tcp("127.0.0.1", 35505, streaming.CLIENT):name("client")

sys.wait(2)

local expected_message = "Hello World"
runner.check(client:send(expected_message), "Failed to send message")
local actual_message = server:receive()

print("Message: ", actual_message)
runner.check(expected_message == actual_message, "Failed to match messages")

server:close()
client:close()

-- Report Results --

runner.report()

