--[[
Copyright (c) 2021, University of Washington
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice,
   this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

3. Neither the name of the University of Washington nor the names of its
   contributors may be used to endorse or promote products derived from this
   software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY OF WASHINGTON AND CONTRIBUTORS
“AS IS” AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE UNIVERSITY OF WASHINGTON OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
--]]

--
-- Notes
--
--[[
  * This file is an HAProxy lua service script which implements service discovery and node orchestration.

  * To load this script, the following configuration must be inserted into the HAProxy config file:
      global
        lua-load /path/to/orchestrator.lua

      frontend orchestrator
        bind :8050
        http-request use-service lua.orchestrator_register if { path /discovery/register }
        http-request use-service lua.orchestrator_lock if { path /discovery/lock }
        http-request use-service lua.orchestrator_unlock if { path /discovery/unlock }
        http-request use-service lua.orchestrator_prometheus if { path /discovery/prometheus }
        http-request use-service lua.orchestrator_health if { path /discovery/health }

  * cURL can be used to test out basic functionality of the endpoints
      % curl -X POST -d "{\"service\":\"test\", \"lifetime\":10, \"address\":\"127.0.0.1\"}" http://127.0.0.1:8050/discovery/register
--]]

--
-- Globals
--
core.log(core.info, "Loading orchestrator...")
package.path = package.path .. ';/usr/local/etc/haproxy/?.lua'

--
-- Imports
--
local json = require("json")
local prettyprint = require("prettyprint")

--
-- Local Functions
--
local function sort_by_locks(registry)
    local addresses = {}
    local num_addresses = 0
    for address,_ in pairs(registry) do
        table.insert(addresses, address)
        num_addresses = num_addresses + 1
    end
    table.sort(addresses, function(address1, address2)
        return registry[address1]["locks"] < registry[address2]["locks"]
    end)
    return addresses, num_addresses
end

--
-- Service Catalog
--
--  {
--      "<service 1>":
--      {
--          "<address 1>":
--          {
--              "service":      "<service 1>",
--              "expiration":   <expiration time in seconds>,
--              "address":      "<address 1>",
--              "locks":        <number of active locks>,
--          }
--          ..
--          "<address n>":
--          {
--              "service":      "<service 1>",
--              "expiration":   <expiration time in seconds>,
--              "address":      "<address n>",
--              "locks":        <number of active locks>,
--          }
--      }
--      ..
--      "<service m>":
--      {
--      ..
--      }
--  }
ServiceCatalog = {}

--
-- Transaction Table
--
--  {
--      <transaction id>: [<service registry>, <address>, <timeout>],
--      ..
--  }
--
TransactionTable = {}
TransactionId = 0

--
-- Statistics Data
--
--  {
--      "numRequests": <number of total requests>,
--      "numComplete": <number of unlocked transactions>,
--      "numFailures": <number of failed requests>,
--      "numTimeouts": <number of transactions that have timed-out>,
--      "memberCounts":
--      {
--          "<service 1>": <number of members>,
--          ..
--          "<service m>": <number of members>
--      }
--  }
--
StatData = {
    numRequests = 0,
    numComplete = 0,
    numFailures = 0,
    numTimeouts = 0,
    numActiveLocks = 0,
    memberCounts = {}
}

--
-- Constants
--
MaxLocksPerNode = 3
ScrubInterval = 1 -- second(s)
MaxTimeout = 600 -- second(s)

--
-- API: /discovery/register
--
--  Register members of cluster
--
--  INPUT:
--  {
--      "service": "<service to join>",
--      "lifetime": <duration that registry lasts in seconds>
--      "address": "<public hostname or ip address of member>",
--  }
--
--  OUTPUT:
--  {
--      "<address>": ["<service>", "<expiration>"]
--  }
--
local function api_register(applet)

    -- process request
    local body = applet:receive()
    local request = json.decode(body)
    local service = request["service"]
    local lifetime = request["lifetime"]
    local address = request["address"]

    -- build service member table
    local member = {
        service = service,
        expiration = os.time() + lifetime,
        address = address,
        locks = 0
    }

    -- update service catalog
    local service_registry = ServiceCatalog[service]
    if service_registry == nil then                             -- if first time service is registered
        ServiceCatalog[service] = {}                            -- register service by adding it to catalog
    elseif service_registry[address] ~= nil then                -- if service already registered
        member["locks"] = service_registry[address]["locks"]    -- preserve number of locks for member
    end
    ServiceCatalog[service][address] = member

    -- send response
    local response = string.format([[{"%s": ["%s", %d]}]], address, service, member["expiration"])
    applet:set_status(200)
    applet:add_header("content-length", string.len(response))
    applet:add_header("content-type", "application/json")
    applet:start_response()
    applet:send(response)

end

--
-- API: /discovery/lock
--
--  Returns up to requested number of nodes for processing a request
--
--  INPUT:
--  {
--      "service": "<service>",
--      "nodesNeeded": <number>,
--      "timeout": <seconds>
--  }
--
--  OUTPUT:
--  {
--      "members": ["<address1>", "<address2>", ...],
--      "transactions": [tx1, tx2, ...]
--  }
--
local function api_lock(applet)

    -- process request
    local body = applet:receive()
    local request = json.decode(body)
    local service = request["service"]
    local nodesNeeded = request["nodesNeeded"]
    local timeout = request["timeout"] < MaxTimeout and request["timeout"] or MaxTimeout
    local expiration = os.time() + timeout

    -- initialize error count
    local error_count = 0

    -- loop through service registry and return addresses with least locks
    local member_list = {} -- list of member addresses returned
    local transaction_list = {} -- list of transaction ids returned
    local service_registry = ServiceCatalog[service]
    if service_registry ~= nil then
        local sorted_addresses, num_adresses = sort_by_locks(service_registry)
        if num_adresses > 0 then
            local i = 1
            while nodesNeeded > 0 do
                -- check if end of list
                if i > num_adresses then i = 1 end
                -- pull out member
                local address = sorted_addresses[i]
                local member = service_registry[address]
                -- check if number of locks exceeds max
                if member["locks"] >= MaxLocksPerNode then
                    if i == 1 then break -- full capacity
                    else i = 0 end -- go back to beginning of sorted list (goes to 1 below)
                else
                    -- create transaction
                    local transaction = {
                        service_registry,
                        address,
                        expiration
                    }
                    -- lock member
                    nodesNeeded = nodesNeeded - 1 -- need one less node now
                    member["locks"] = member["locks"] + 1 -- member has one more lock
                    table.insert(member_list, string.format('"%s"', member["address"])) -- populate member list that gets returned
                    table.insert(transaction_list, TransactionId) -- populate list of transaction ids that get returned
                    TransactionTable[TransactionId] = transaction -- register transaction
                    TransactionId = TransactionId + 1
                end
                -- goto next entry in address list
                i = i + 1
            end
        else
            core.log(core.err, string.format("No addresses found in registry %s", service))
            error_count = error_count + 1
        end
    else
        core.log(core.err, string.format("Service %s not found", service))
        error_count = error_count + 1
    end

    -- update statistics
    StatData["numRequests"] = StatData["numRequests"] + 1
    StatData["numFailures"] = StatData["numFailures"] + error_count

    -- send response
    local response = string.format([[{"members": [%s], "transactions": [%s]}]], table.concat(member_list, ","), table.concat(transaction_list, ","))
    applet:set_status(200)
    applet:add_header("content-length", string.len(response))
    applet:add_header("content-type", "application/json")
    applet:start_response()
    applet:send(response)

end

--
-- API: /discovery/unlock
--
--  Releases lock for members associated with provided transactions
--
--  INPUT:
--  {
--      "transactions": [<id1>, <id2>, ...]
--  }
--
--  OUTPUT:
--  {
--      "complete": <number of completed transactions from this request>,
--      "fail": <number of transactions that failed to be processed from this request>
--  }
--
local function api_unlock(applet)

    -- process request
    local body = applet:receive()
    local request = json.decode(body)
    local transactions = request["transactions"]

    -- initialize count
    local error_count = 0
    local num_transactions = 0

    -- loop through each transaction
    for _,id in pairs(transactions) do
        num_transactions = num_transactions + 1
        local transaction = TransactionTable[id]
        if transaction ~= nil then
            local service_registry = transaction[1]
            local address = transaction[2]
            local member = service_registry[address]
            if member ~= nil then
                -- unlock member
                if member["locks"] > 0 then
                    member["locks"] = member["locks"] - 1
                else
                    core.log(core.err, string.format("Transaction %d unlocked on address %s with no locks", id, address))
                    error_count = error_count + 1
                end
            else
                core.log(core.err, string.format("Address %s is not registered to service", address))
                error_count = error_count + 1
            end
            -- remove transaction from transaction table
            TransactionTable[id] = nil
        else
            core.log(core.err, string.format("Missing transaction id %d", id))
            error_count = error_count + 1
        end
    end

    -- update statistics
    StatData["numComplete"] = StatData["numComplete"] + num_transactions
    StatData["numFailures"] = StatData["numFailures"] + error_count

    -- send response
    local response = string.format([[{"complete": %d, "fail": %d}]], num_transactions, error_count)
    applet:set_status(200)
    applet:add_header("content-length", string.len(response))
    applet:add_header("content-type", "application/json")
    applet:start_response()
    applet:send(response)

end

--
-- API: /discovery/prometheus
--
--  Provide metrics to prometheus scraper
--
local function api_prometheus(applet)

    -- build initial response
    local response = string.format([[
# TYPE num_requests counter
num_requests %d

# TYPE num_complete counter
num_complete %d

# TYPE num_failures counter
num_failures %d

# TYPE num_timeouts counter
num_timeouts %d

# TYPE num_active_locks counter
num_active_locks %d
]], StatData["numRequests"], StatData["numComplete"], StatData["numFailures"], StatData["numTimeouts"], StatData["numActiveLocks"])

    -- add member counts to response
    for member_metric,_ in pairs(StatData["memberCounts"]) do
        response = response .. string.format([[

# TYPE %s counter
%s %d
]], member_metric, member_metric, StatData["memberCounts"][member_metric])
    end

    -- send response
    applet:set_status(200)
    applet:add_header("content-length", string.len(response))
    applet:add_header("content-type", "text/plain")
    applet:start_response()
    applet:send(response)
end

--
-- API: /discovery/health
--
local function api_health(applet)

    local response = string.format([[{"health":true}]])
    applet:set_status(200)
    applet:add_header("content-length", string.len(response))
    applet:add_header("content-type", "application/json")
    applet:start_response()
    applet:send(response)

end

--
-- API: /discovery/status
--
--  Returns status of cluster
--
--  INPUT:
--  {
--      "service": "<service name>"
--  }
--
--  OUTPUT:
--  {
--      "nodes": <number of registered nodes>,
--  }
--
local function api_status(applet)

    -- process request
    local body = applet:receive()
    local request = json.decode(body)
    local service = request["service"]

    -- initialize error count
    local error_count = 0

    -- loop through service registry and return number of addresses
    local num_addresses = 0
    local service_registry = ServiceCatalog[service]
    if service_registry ~= nil then
        for _,_ in pairs(service_registry) do
            num_addresses = num_addresses + 1
        end
    else
        core.log(core.err, string.format("Service %s not found", service))
        error_count = error_count + 1
    end

    -- update statistics
    StatData["numFailures"] = StatData["numFailures"] + error_count

    -- send response
    local response = string.format([[{"nodes": %d}]], num_addresses)
    applet:set_status(200)
    applet:add_header("content-length", string.len(response))
    applet:add_header("content-type", "application/json")
    applet:start_response()
    applet:send(response)

end

--
-- Task: scrubber
--
local function backgroud_scrubber()
    while true do
        core.msleep(ScrubInterval * 1000)
        local now = os.time()
        -- scrub expired member registrations
        for service, service_registry in pairs(ServiceCatalog) do
            -- build list of members to delete (expire)
            local total_num_members = 0
            local members_to_delete = {}
            for address, member in pairs(service_registry) do
                total_num_members = total_num_members + 1
                if member["expiration"] <= now then
                    table.insert(members_to_delete, address)
                end
            end
            -- delete (expire) members
            for _,address in pairs(members_to_delete) do
                service_registry[address] = nil
            end
            -- set member count
            StatData["memberCounts"][service.."_members"] = total_num_members
        end
        -- scrub timed-out transactions
        local num_timeouts = 0
        local num_transactions = 0
        local transactions_to_delete = {}
        for id, transaction in pairs(TransactionTable) do
            local service_registry = transaction[1]
            local address = transaction[2]
            local expiration = transaction[3]
            local member = service_registry[address]
            if expiration <= now then
                -- add id to list of transactions to delete (time-out)
                table.insert(transactions_to_delete, id)
                num_timeouts = num_timeouts + 1
                -- decrement associated lock
                if member ~= nil then
                    if member["locks"] > 0 then
                        member["locks"] = member["locks"] - 1
                    else
                        core.log(core.err, string.format("Transaction %d timed-out on %s with no locks", id, address))
                    end
                else
                    core.log(core.err, string.format("Transaction %d associated with an unknown address %s", id, address))
                end
            else
                num_transactions = num_transactions + 1
            end
        end
        -- delete (time-out) transactions
        for _,id in pairs(transactions_to_delete) do
            TransactionTable[id] = nil
        end
        -- update statistics
        StatData["numTimeouts"] = StatData["numTimeouts"] + num_timeouts
        StatData["numActiveLocks"] = num_transactions
    end
end

--
-- Fetch: next_node
--
local function orchestrator_next_node(txn, service)
    local service_registry = ServiceCatalog[service]
    if service_registry ~= nil then
        -- sort members by number of locks
        local sorted_addresses, num_addresses = sort_by_locks(service_registry)
        if num_addresses > 0 then
            -- get set of nodes with same minimal number of locks
            local min_num_locks = service_registry[sorted_addresses[1]]["locks"]
            local num_members_with_min_locks = 1
            local i = 2
            while i <= num_addresses do
                if service_registry[sorted_addresses[i]]["locks"] <= min_num_locks then
                    num_members_with_min_locks = num_members_with_min_locks + 1
                end
                i = i + 1
            end
            -- choose address randomly from set of minimally locked members
            return sorted_addresses[math.random(1, num_members_with_min_locks)]
        else
            core.log(core.err, string.format("No nodes available on service %s", service))
        end
    else
        core.log(core.err, string.format("Unknown service %s", service))
    end
    return "127.0.0.1:9081"
end

--
-- Converter: extract_ip
--
local function orchestrator_extract_ip(address)
    local protocol, _, ip, __, port = address:match('(.*)(://)(.*)(:)(.*)')
    return ip
end

--
-- Converter: extract_port
--
local function orchestrator_extract_port(address)
    local protocol, _, ip, __, port = address:match('(.*)(://)(.*)(:)(.*)')
    return port
end

--
-- Register with HAProxy
--
core.register_service("orchestrator_register", "http", api_register)
core.register_service("orchestrator_lock", "http", api_lock)
core.register_service("orchestrator_unlock", "http", api_unlock)
core.register_service("orchestrator_prometheus", "http", api_prometheus)
core.register_service("orchestrator_health", "http", api_health)
core.register_service("orchestrator_status", "http", api_status)
core.register_task(backgroud_scrubber)
core.register_fetches("next_node", orchestrator_next_node)
core.register_converters("extract_ip", orchestrator_extract_ip)
core.register_converters("extract_port", orchestrator_extract_port)
