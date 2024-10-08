local runner = require("test_executive")
local console = require("console")

-- Cluster Socket Unit Test --

local server = streaming.cluster(streaming.WRITER, streaming.QUEUE, "127.0.0.1", 34503, streaming.SERVER, "inq"):name("clusterServer")
local client = streaming.cluster(streaming.READER, streaming.QUEUE, "127.0.0.1", 34503, streaming.CLIENT, "outq"):name("clusterClient")
local writer = streaming.writer(server):name("clusterWriter")
local reader = streaming.reader(client):name("clusterReader")


reader:block(true)
local attempts = 10
while attempts > 0 and not client:connected() do
    attempts = attempts - 1
    print("Waiting for cluster socket to connect...", 10 - attempts)
    sys.wait(1)
end

local inq = msg.publish("inq")
local outq = msg.subscribe("outq")

runner.check(inq ~= nil)
runner.check(outq ~= nil)

runner.check(inq:sendstring("HELLO WORLD 1"))
runner.check(inq:sendstring("HELLO WORLD 2"))
runner.check(inq:sendstring("HELLO WORLD 3"))

message1 = outq:recvstring(5000)
message2 = outq:recvstring(5000)
message3 = outq:recvstring(5000)

print("Received message 1: " .. message1)
print("Received message 2: " .. message2)
print("Received message 3: " .. message3)

runner.compare(message1, "HELLO WORLD 1")
runner.compare(message2, "HELLO WORLD 2")
runner.compare(message3, "HELLO WORLD 3")

-- Clean Up --

writer:destroy()
reader:destroy()
server:destroy()
client:destroy()

-- Report Results --

runner.report()

