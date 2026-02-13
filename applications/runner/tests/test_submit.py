from runner import submit_handler, report_jobs_handler, report_queue_handler
import base64

lua_script = base64.b64encode("""
print("Hello World")
return "Nice to meet you", true
""".encode()).decode()

def test_missing_name():
    rsps = submit_handler({}, None)
    assert rsps["status"] == False

def test_invalid_name():
    rsps = submit_handler({"name": 1}, None)
    assert rsps["status"] == False

def test_empty_script():
    rsps = submit_handler({"name": "hello_world", "script":""}, None)
    assert rsps["status"] == False

def test_invalid_args_list():
    rsps = submit_handler({"name": "hello_world", "script":lua_script, "args_list": "test1"}, None)
    assert rsps["status"] == False

def test_invalid_vcpus():
    rsps = submit_handler({"name": "hello_world", "script":lua_script, "args_list": [""], "vcpus": 50}, None)
    assert rsps["status"] == False

def test_invalid_memory():
    rsps = submit_handler({"name": "hello_world", "script":lua_script, "args_list": [""], "memory": "lots"}, None)
    assert rsps["status"] == False

#def test_nominal():
#    rsps = submit_handler({
#        "name": "hello_world",
#        "script": lua_script,
#        "args_list": [""]
#    }, None)
#    print(rsps)
