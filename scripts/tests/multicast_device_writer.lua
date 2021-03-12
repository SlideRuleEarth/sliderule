local runner = require("test_executive")
local console = require("console")

console.monitor:config(core.INFO)

-- UdpSocket/Multicast/DeviceWriter Unit Test --

-- create writer

sockoutq = msg.publish("sockoutq")
runner.check(sockoutq, "Failed to create socket output queue")

client = core.udp("239.255.0.1", 35028, core.CLIENT)
writer = core.writer(client, "sockoutq")
sys.wait(2)

-- create reader

sockinq = msg.subscribe("sockinq")
runner.check(sockinq)

server = core.udp("0.0.0.0", 35028, core.SERVER, "239.255.0.1")
reader = core.reader(server, "sockinq")
sys.wait(2)

-- send message

expected_message = "Hello World"
runner.check(sockoutq:sendstring(expected_message), "Failed to send message")
actual_message = sockinq:recvstring(5000)

-- check results

print("Message: ", actual_message)
runner.check(expected_message == actual_message, "Failed to match messages")

-- clean up

client:close()
server:close()

-- Report Results --

runner.report()

