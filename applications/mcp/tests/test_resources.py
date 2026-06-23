import json
from mcp import lambda_gateway
from tests.utility import construct_request

#
# List Resources
#
def test_list_resources():
    rsps = lambda_gateway(construct_request(["member"], "/sliderule", "resources/list", {}, 0), None)
    body = json.loads(rsps["body"])
    assert len(body["result"]["resources"]) == 32
    assert body["result"]["resources"][0]["uri"] == "sliderule://mcp/workflows.md"

#
# List Templates
#
def test_list_templates():
    rsps = lambda_gateway(construct_request(["member"], "/sliderule", "resources/templates/list", {}, 0), None)
    body = json.loads(rsps["body"])
    assert len(body["result"]["resourceTemplates"]) == 3
    assert body["result"]["resourceTemplates"][0]["uriTemplate"] == "sliderule://mcp/datasets/{uuid}"

#
# Read MCP Resource
#
def test_read_mcp_resource():
    rsps = lambda_gateway(construct_request(["member"], "/sliderule", "resources/read", {"uri": "sliderule://mcp/workflows.md"}, 0), None)
    body = json.loads(rsps["body"])
    assert len(body["result"]["contents"][0]["text"]) > 600

#
# Missing MCP Resource
#
def test_missing_mcp_resource():
    rsps = lambda_gateway(construct_request(["member"], "/sliderule", "resources/read", {"uri": "sliderule://mcp/not_there"}, 0), None)
    body = json.loads(rsps["body"])
    assert body["error"]["code"] == -32603

#
# Read OpenAPI Resource
#
def test_read_openapi_resource():
    rsps = lambda_gateway(construct_request(["member"], "/sliderule", "resources/read", {"uri": "sliderule://openapi/sliderule/openapi.json"}, 0), None)
    body = json.loads(rsps["body"])
    spec = json.loads(body["result"]["contents"][0]["text"])
    assert len(spec) > 1
    assert "openapi" in spec
    assert "info" in spec

#
# Missing OpenAPI Resource
#
def test_missing_openapi_resource():
    rsps = lambda_gateway(construct_request(["member"], "/sliderule", "resources/read", {"uri": "sliderule://openapi/not_there"}, 0), None)
    body = json.loads(rsps["body"])
    assert body["error"]["code"] == -32603

#
# Read Python Docs Resource
#
def test_read_python_docs_resource():
    rsps = lambda_gateway(construct_request(["member"], "/sliderule", "resources/read", {"uri": "sliderule://docs/python/getting_started/Install"}, 0), None)
    body = json.loads(rsps["body"])
    assert len(body["result"]["contents"][0]["text"]) > 600

#
# Missing Python Docs Resource
#
def test_missing_python_docs_resource():
    rsps = lambda_gateway(construct_request(["member"], "/sliderule", "resources/read", {"uri": "sliderule:/docs/python/not_there"}, 0), None)
    body = json.loads(rsps["body"])
    assert body["error"]["code"] == -32603

#
# Read Python Example Resource
#
def test_read_python_example_resource():
    rsps = lambda_gateway(construct_request(["member"], "/sliderule", "resources/read", {"uri": "sliderule://examples/python/atl06_glims_subset"}, 0), None)
    body = json.loads(rsps["body"])
    assert len(body["result"]["contents"][0]["text"]) > 600

#
# Missing Python Docs Resource
#
def test_missing_python_example_resource():
    rsps = lambda_gateway(construct_request(["member"], "/sliderule", "resources/read", {"uri": "sliderule:/examples/python/not_there"}, 0), None)
    body = json.loads(rsps["body"])
    assert body["error"]["code"] == -32603
