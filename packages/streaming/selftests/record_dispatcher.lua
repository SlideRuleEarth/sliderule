local runner = require("test_executive")

-- Requirements --

if not __legacy__ then
	return runner.skip()
end

-- Self Test --

runner.command("DEFINE test.rec id 8")
runner.command("ADD_FIELD test.rec id INT32 0 1 NATIVE")
runner.command("ADD_FIELD test.rec counter INT32 4 1 NATIVE")

local idmetric = streaming.metric("id", "dispatcher_metricq"):name("idmetric")
idmetric:pbtext(true):pbname(true)

local countermetric = streaming.metric("counter", "dispatcher_metricq"):name("countermetric")
countermetric:pbtext(true):pbname(true)

local r = streaming.dispatcher("dispatcher_inputq"):name("dispatcher")
r:attach(idmetric, "test.rec"):attach(countermetric, "test.rec"):run()

local inputq = msg.publish("dispatcher_inputq")
local metricq = msg.subscribe("dispatcher_metricq")

-- send test records
local expected_totals = {id=0, counter=0}
for i=1,100,1 do
	local testrec1 = msg.create(string.format('test.rec id=1000 counter=%d', i))
	inputq:sendrecord(testrec1)
	local testrec2 = msg.create(string.format('test.rec id=2000 counter=%d', i))
	inputq:sendrecord(testrec2)
	expected_totals["id"] = expected_totals["id"] + 3000
	expected_totals["counter"] = expected_totals["counter"] + (i * 2)
end

sys.lsmsgq()

-- receive metrics
local actual_totals = {}
actual_totals["test.rec.id"]=0
actual_totals["test.rec.counter"]=0

for i=1,400,1 do
	local metric = metricq:recvrecord(1000)
	if metric then
	    local name = metric:getvalue("NAME")
	    local value = metric:getvalue("VALUE")
	    actual_totals[name] = actual_totals[name] + value
    end
end

-- compare results
runner.assert(expected_totals["id"] == actual_totals["test.rec.id"])
runner.assert(expected_totals["counter"] == actual_totals["test.rec.counter"])

-- Clean Up --

r:destroy()
idmetric:destroy()
countermetric:destroy()

-- Report Results --

runner.report()

