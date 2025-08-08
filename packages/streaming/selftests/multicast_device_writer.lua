local runner = require("test_executive")

-- Self Test --

-- create writer

local sockoutq = msg.publish("sockoutq")
runner.assert(sockoutq, "Failed to create socket output queue")

local client = streaming.udp("239.255.0.1", 35028, streaming.CLIENT)
local writer = streaming.writer(client, "sockoutq")
sys.wait(2)

-- create reader

local sockinq = msg.subscribe("sockinq")
runner.assert(sockinq)

local server = streaming.udp("0.0.0.0", 35028, streaming.SERVER, "239.255.0.1")
local reader = streaming.reader(server, "sockinq")
sys.wait(2)

-- send message

local expected_message = "Hello World"
runner.assert(sockoutq:sendstring(expected_message), "Failed to send message")
local actual_message = sockinq:recvstring(5000)

-- check results

print("Message: ", actual_message)
runner.assert(expected_message == actual_message, "Failed to match messages")

-- Clean Up --

client:close()
server:close()

-- Report Results --

runner.report()
