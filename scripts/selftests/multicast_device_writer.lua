local runner = require("test_executive")
local console = require("console")

-- UdpSocket/Multicast/DeviceWriter Unit Test --

-- create writer

local sockoutq = msg.publish("sockoutq")
runner.check(sockoutq, "Failed to create socket output queue")

local client = streaming.udp("239.255.0.1", 35028, streaming.CLIENT)
local writer = streaming.writer(client, "sockoutq")
sys.wait(2)

-- create reader

local sockinq = msg.subscribe("sockinq")
runner.check(sockinq)

local server = streaming.udp("0.0.0.0", 35028, streaming.SERVER, "239.255.0.1")
local reader = streaming.reader(server, "sockinq")
sys.wait(2)

-- send message

local expected_message = "Hello World"
runner.check(sockoutq:sendstring(expected_message), "Failed to send message")
local actual_message = sockinq:recvstring(5000)

-- check results

print("Message: ", actual_message)
runner.check(expected_message == actual_message, "Failed to match messages")

-- clean up

client:close()
server:close()

-- Report Results --

runner.report()

