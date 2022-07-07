# Orchestrator
#
# Provides service discovery for internal cluster of serve nodes
#
# Example Usage:
#   $ python orchestrator.py
#   $ curl -X POST -d "{\"service\": \"sliderule\", \"lifetime\": 30, \"name\":\"12.33.32.21\"}" http://localhost:8050
#   $ curl -X GET -d "{\"service\": \"sliderule\"}" http://localhost:8050

#
# Imports
#
from http.server import BaseHTTPRequestHandler, HTTPServer
from threading import Thread, Lock
from time import time, sleep
import json

#
# Global Data
#
serverSettings = {
    'host': '0.0.0.0',
    'port': 8050,
    'maxLocksPerNode': 3,
    'scrubInterval': 5 # seconds
}

serverLock = Lock()
serviceCatalog = {}
transactionTable = {}
transactionId = 0

statLock = Lock()
statData = {
    "numRequests" : 0,
    "numComplete" : 0,
    "numFailures": 0,
    "numTimeouts": 0,
}

#
# Orchestrator
#
class Orchestrator(BaseHTTPRequestHandler):

    #
    # API: Lock
    #
    def api_lock(self):
        '''
        Returns up to requested number of nodes for processing a request

        Input:
        {
            "service": "<service>",
            "nodesNeeded": <number>,
            "timeout": <seconds>
        }

        Output:
        {
            "members": ["<hostname1>", "<hostname2>", ...],
            "transactions": [tx1, tx2, ...]
        }
        '''
        global serviceCatalog,transactionId
        request_count = 0
        error_count = 0
        member_list = []
        transaction_list = []
        # process request
        try:
            # extract request
            content_length = int(self.headers['Content-Length'])
            data = self.rfile.read(content_length).decode('utf-8')
            request = json.loads(data)
            nodes_needed = request["nodesNeeded"]
            request_count = nodes_needed
            node_timeout = time() + request["timeout"]
            # build member list
            with serverLock:
                node_list = serviceCatalog[request['service']].items()
                if request['service'] in serviceCatalog and len(node_list) > 0:
                    node_list = sorted(node_list, key=lambda x: x[1]["numLocks"])
                    i = 0
                    while nodes_needed > 0:
                        # check if end of list
                        if i >= len(node_list):
                            i = 0
                        # pull out node info
                        client, member = node_list[i]
                        # check if num locks exceeded max
                        if member["numLocks"] >= serverSettings["maxLocksPerNode"]:
                            if i == 0:
                                break # full capacity
                            else:
                                i = 0 # go back to beginning of the list
                        else:
                            nodes_needed -= 1
                            # update lock count for member
                            member['numLocks'] += 1
                            # add member and transaction to list returned in response
                            member_list.append('"%s"' % (member['name']))
                            transaction_list.append(transactionId)
                            # add member to transaction queue
                            transactionTable[transactionId] = (serviceCatalog[request['service']], client, node_timeout)
                            transactionId += 1
                        # goto next member
                        i += 1

            # send successful response
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.end_headers()
            response = "{\"members\": [%s], \"transactions\": [%s]}" % (', '.join(member_list), ', '.join([str(tx) for tx in transaction_list]))
            self.wfile.write(bytes(response, "utf-8"))
        except:
            error_count = 1
            # send error response
            self.send_response(500)
            self.end_headers()
        # update stats
        with statLock:
            statData["numRequests"] += request_count
            statData["numFailures"] += error_count

    #
    # API: Unlock
    #
    def api_unlock(self):
        '''
        Releases lock for members associated with provided transactions

        Input:
        {
            "transactions": [<id1>, <id2>, ...]
        }
        '''
        global transactionTable, statData
        complete_count = 0
        error_count = 0
        # process request
        try:
            # extract request
            content_length = int(self.headers['Content-Length'])
            data = self.rfile.read(content_length).decode('utf-8')
            request = json.loads(data)
            transactions = request["transactions"]
            complete_count = len(transactions)
            # unlock members associated with provided transactions
            with serverLock:
                for id in transactions:
                    tx = transactionTable[id]
                    service = tx[0]
                    client = tx[1]
                    if client in service:
                        if service[client]["numLocks"] > 0:
                            service[client]["numLocks"] -= 1
                        else:
                            print("ERROR: transaction unlocked on member with no locks (%s)" % (client))
                    else:
                        print("ERROR: unknown client (%s)" % (client))
                    del transactionTable[id]
            # send successful response
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.end_headers()
            response = "{\"complete\": %d, \"fail\": %d}" % (complete_count, error_count)
            self.wfile.write(bytes(response, "utf-8"))
        except:
            error_count = 1
            # send error response
            self.send_response(500)
            self.end_headers()
        # update stats
        with statLock:
            statData["numComplete"] += complete_count
            statData["numFailures"] += error_count

    #
    # API: Prometheus
    #
    def api_prometheus(self):
        '''
        Provide metrics to prometheus scraper
        '''
        global statData
        # build response
        response = """
# TYPE num_requests counter
num_requests {}

# TYPE num_complete counter
num_complete {}

# TYPE num_failures counter
num_failures {}

# TYPE num_timeouts counter
num_timeouts {}

# TYPE num_active_locks counter
num_active_locks {}
""".format(statData["numRequests"], statData["numComplete"], statData["numFailures"], statData["numTimeouts"], len(transactionTable))
        # send response
        self.send_response(200)
        self.send_header("Content-Type", "text/plain")
        self.end_headers()
        self.wfile.write(bytes(response, "utf-8"))

    #
    # API: Health
    #
    def api_health(self):
        '''
        Health check
        '''
        # send response
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.end_headers()
        with serverLock: # exercise server lock
            response = "{\"health\":true}"
        self.wfile.write(bytes(response, "utf-8"))

    #
    # HTTP: GET
    #
    def do_GET(self):
        if self.path == "/lock":
            self.api_lock()
        elif self.path == "/unlock":
            self.api_unlock()
        elif self.path == "/prometheus":
            self.api_prometheus()
        elif self.path == "/health":
            self.api_health()

    #
    # HTTP: POST
    #
    def do_POST(self):
        '''
        Register members of cluster

        Input:
        {
            "service": "<service to join>",
            "lifetime": <duration that registry lasts in seconds>
            "name": "<public hostname or ip address of member>",
        }
        '''
        valid = True
        # process request
        try:
            # extract request
            content_length = int(self.headers['Content-Length'])
            data = self.rfile.read(content_length).decode('utf-8')
            request = json.loads(data)
            # build member structure
            member = {
                'client': self.client_address[0],
                'service': request['service'],
                'expiration': time() + request['lifetime'],
                'name': request['name'],
                'numLocks': 0
            }
            # add member to service serviceCatalog
            with serverLock:
                if member['service'] not in serviceCatalog:
                    serviceCatalog[member['service']] = {}
                if member['client'] in serviceCatalog[member['service']]:
                    member["numLocks"] = serviceCatalog[member['service']][member['client']]["numLocks"]
                serviceCatalog[member['service']][member['client']] = member
            # send successful response
            self.send_response(200)
            self.send_header("Content-Type", "application/json")
            self.end_headers()
            response = "{\"%s\": [\"%s\", \"%s\"]}" % (request['name'], request['service'], member['client'])
            self.wfile.write(bytes(response, "utf-8"))
        except:
            # send error response
            self.send_response(500)
            self.end_headers()

#
# Orchestrator Thread
#
def orchestrator_thread(host, port, orchestrator):
    with HTTPServer((host, port), orchestrator) as server:
        print("Orchestrator started at http://%s:%s" % (host, port))
        server.serve_forever()
    print("Orchestrator stopped at http://%s:%s" % (host, port))

#
# Scrubber Thread
#
def scrubber_thread():
    global serverSettings, serviceCatalog, transactionTable
    while True:
        sleep(serverSettings["scrubInterval"])
        now = time()
        # Scrub Expired Member Registrations
        members_to_delete = []
        for service in serviceCatalog.values():
            with serverLock:
                for _, member in service.items():
                    if member['expiration'] <= now:
                        members_to_delete.append(member['client'])
                for member in members_to_delete:
                    del service[member]
        # Scrub Timed-Out Transactions
        num_timeouts = 0
        transactions_to_delete = []
        with serverLock:
            for id, tx in transactionTable.items():
                service = tx[0]
                client = tx[1]
                timeout = tx[2]
                if timeout <= now:
                    if client in service:
                        if service[client]["numLocks"] > 0:
                            service[client]["numLocks"] -= 1
                        else:
                            print("ERROR: transaction timed-out on member with no locks (%s)" % (client))
                    else:
                        print("ERROR: unknown client (%s)" % (client))
                    transactions_to_delete.append(id)
                    num_timeouts += 1
            for id in transactions_to_delete:
                del transactionTable[id]
        # Update Statistics
        with statLock:
            statData["numTimeouts"] += num_timeouts
#
# Main
#
if __name__ == "__main__":

    scrubber = Thread(target=scrubber_thread)
    scrubber.start()

    server = Thread(target=orchestrator_thread, args=(serverSettings['host'], serverSettings['port'], Orchestrator))
    server.start()

    server.join()
    scrubber.join()
