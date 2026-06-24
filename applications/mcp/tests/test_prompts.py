import json
from mcp import lambda_gateway
from tests.utility import construct_request

#
# List Prompts
#
def test_list_prompts():
    rsps = lambda_gateway(construct_request(["member"], "/sliderule", "prompts/list", {}, 0), None)
    body = json.loads(rsps["body"])
    assert len(body["result"]["prompts"]) == 2
    assert body["result"]["prompts"][0]["name"] == "generate_dataset"

#
# Generate Prompt
#
def test_read_mcp_resource():
    rsps = lambda_gateway(construct_request(["member"], "/sliderule", "prompts/get", {"name": "generate_python", "arguments": {"question": "how do I write code"}}, 0), None)
    body = json.loads(rsps["body"])
    assert len(body["result"]["messages"][0]["content"]["text"]) > 10

#
# Missing Prompt
#
def test_missing_mcp_resource():
    rsps = lambda_gateway(construct_request(["member"], "/sliderule", "prompts/get", {"name": "missing_prompt", "arguments": {"question": "how do I write code"}}, 0), None)
    body = json.loads(rsps["body"])
    assert body["error"]["code"] == -32603
