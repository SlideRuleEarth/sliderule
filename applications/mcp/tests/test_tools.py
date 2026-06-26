import json
from mcp import lambda_gateway
from tests.utility import CLUSTER, construct_request

#
# List Tools
#
def test_list_tools():
    rsps = lambda_gateway(construct_request(["member"], f"/{CLUSTER}", "tools/list", {}, 0), None)
    body = json.loads(rsps["body"])
    assert len(body["result"]["tools"]) > 10
    assert body["result"]["tools"][0]["name"] == "icesat2/atl03/density_metrics"

#
# Call Tool
#
#def test_call_tool():
#    rsps = lambda_gateway(construct_request(["member"], f"/{CLUSTER}", "tools/call", {"name": "icesat2/atl03/subset", "arguments": {}}, 0), None)
#    body = json.loads(rsps["body"])
#    assert len(body["result"]["content"][0]["text"]) > 10

#
# Missing Tool
#
def test_missing_tool():
    rsps = lambda_gateway(construct_request(["member"], f"/{CLUSTER}", "tools/call", {"name": "missing/tool", "arguments": {}}, 0), None)
    body = json.loads(rsps["body"])
    assert body["error"]["code"] == -32603
