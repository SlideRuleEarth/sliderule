local runner = require("test_executive")
local console = require("console")

console.monitor:config(core.INFO)

-- Record Dispatcher Unit Test Setup --

runner.command("DEFINE test.rec id 8")
runner.command("ADD_FIELD test.rec id INT32 0 1 NATIVE")
runner.command("ADD_FIELD test.rec counter INT32 4 1 NATIVE")

local idmetric = core.metric("id", "dispatcher_metricq")
idmetric:pbtext(true)
idmetric:pbname(true)
idmetric:name("idmetric")

local countermetric = core.metric("counter", "dispatcher_metricq")
countermetric:pbtext(true)
countermetric:pbname(true)
countermetric:name("countermetric")

local r = core.dispatcher("dispatcher_inputq")
r:name("dispatcher")
r:attach(idmetric, "test.rec")
r:attach(countermetric, "test.rec")
r:run()

local inputq = msg.publish("dispatcher_inputq")
local metricq = msg.subscribe("dispatcher_metricq")

-- Send Test Records --

expected_totals = {id=0, counter=0}
for i=1,100,1 do
	testrec1 = msg.create(string.format('test.rec id=1000 counter=%d', i))
	inputq:sendrecord(testrec1)
	testrec2 = msg.create(string.format('test.rec id=2000 counter=%d', i))
	inputq:sendrecord(testrec2)
	expected_totals["id"] = expected_totals["id"] + 3000
	expected_totals["counter"] = expected_totals["counter"] + (i * 2)
end

sys.lsmsgq()

-- Receive Metrics --

actual_totals = {}
actual_totals["test.rec.id"]=0
actual_totals["test.rec.counter"]=0

for i=1,400,1 do
	metric = metricq:recvrecord(1000)
	if metric then
	    name = metric:getvalue("NAME")
	    value = metric:getvalue("VALUE")
	    actual_totals[name] = actual_totals[name] + value
    end
end

-- Compare Results --

runner.check(expected_totals["id"] == actual_totals["test.rec.id"])
runner.check(expected_totals["counter"] == actual_totals["test.rec.counter"])

-- Clean Up --

r:destroy()
idmetric:destroy()
countermetric:destroy()

-- Report Results --

runner.report()

