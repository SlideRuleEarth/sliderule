local runner = require("test_executive")

-- Cluster Socket Unit Test --

local server = core.cluster(core.WRITER, core.QUEUE, "127.0.0.1", 34503, core.SERVER, "inq")
local client = core.cluster(core.READER, core.QUEUE, "127.0.0.1", 34503, core.CLIENT, "outq")

local writer = core.writer(server)
local reader = core.reader(client)

reader:block(enable)
local attempts = 10
while attempts > 0 and not client:connected() do
    attempts = attempts - 1
    print("Waiting for cluster socket to connect...", 10 - attempts)
    sys.wait(1)
end

--runner.command("NEW DEVICE_WRITER ClusterServer CLUSTER QUEUE 127.0.0.1 34503 SERVER inq NULL")
--runner.command("NEW DEVICE_READER ClusterClient CLUSTER QUEUE 127.0.0.1 34503 CLIENT outq NULL")
--runner.command("ClusterClient::CONFIG_BLOCK ENABLE")
--runner.command("ClusterClient::WAIT_ON_CONNECT 1 5") -- give time for the connection to occur

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

-- Report Results --

runner.report()

