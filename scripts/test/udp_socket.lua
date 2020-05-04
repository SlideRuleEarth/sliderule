local runner = require("test_executive")
local console = require("console")

-- UdpSocket Unit Test --

server = core.udp("127.0.0.1", 35505, core.SERVER)
client = core.udp("127.0.0.1", 35505, core.CLIENT)

server:name("server")
client:name("client")

sys.wait(2)

expected_message = "Hello World"
runner.check(client:send(expected_message), "Failed to send message")
actual_message = server:receive()

print("Message: ", actual_message)
runner.check(expected_message == actual_message, "Failed to match messages")

server:close()
client:close()

-- Report Results --

runner.report()

