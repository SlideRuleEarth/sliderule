local runner = require("test_executive")
local console = require("console")
console.logger:config(core.INFO)
-- Cluster Socket Unit Test --

local server = core.cluster(core.WRITER, core.QUEUE, "127.0.0.1", 34503, core.SERVER, "inq")
local client = core.cluster(core.READER, core.QUEUE, "127.0.0.1", 34503, core.CLIENT, "outq")

local writer = core.writer(server)
local reader = core.reader(client)

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

server:destroy()
client:destroy()
writer:destroy()
reader:destroy()

-- Report Results --

runner.report()

