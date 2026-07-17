import json
import time
from mcp import lambda_gateway
from tests.utility import CLUSTER, construct_request

#
# ATL03 Subset
#
def test_atl03_subset(local):

    # make tool call
    aoi = [ { "lat": -80.75, "lon": -70.00 },
            { "lat": -81.00, "lon": -70.00 },
            { "lat": -81.00, "lon": -65.00 },
            { "lat": -80.75, "lon": -65.00 },
            { "lat": -80.75, "lon": -70.00 } ]
    resources = ["ATL03_20181019065445_03150111_006_02.h5"]
    rsps = lambda_gateway(construct_request(["member"], f"/{CLUSTER}", "tools/call", {"name": "icesat2/atl03/subset", "arguments": {
        "track": 1,
        "cnf": 4,
        "poly": aoi,
        "resources": resources
    }}, 0), None)
    body = json.loads(rsps["body"])
    job_uri = body["result"]["content"][0]["resource"]["uri"]

    # read job
    seconds_to_wait = 10
    while seconds_to_wait > 0:
        rsps = lambda_gateway(construct_request(["member"], f"/{CLUSTER}", "resources/read", {"uri": job_uri}, 0), None)
        body = json.loads(rsps["body"])
        content = json.loads(body["result"]["contents"][0]["text"])
        status = content["status"]
        if status == "running":
            print(f"Waiting {seconds_to_wait} seconds for job to complete: {job_uri}")
            seconds_to_wait -= 1
            time.sleep(1)
        elif status == "complete":
            print(f"Job complete: {job_uri}")
            break
        elif status == "failed":
            assert False, f"Failed to execute job: {job_uri}"
        else:
            assert False, f"Invalid state of <{status}> for job: {job_uri}"
    dataset_uri = content["dataset"]
    assert dataset_uri, f"Failed to complete job"

    # read dataset
    rsps = lambda_gateway(construct_request(["member"], f"/{CLUSTER}", "resources/read", {"uri": dataset_uri}, 0), None)
    body = json.loads(rsps["body"])
    content = json.loads(body["result"]["contents"][0]["text"])
    assert content["num_rows"] == 445500
    assert len(content["columns"]) == 18
