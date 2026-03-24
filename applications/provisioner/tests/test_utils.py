from provisioner import *
import pytest

#
# Test Cluster Name
#
def test_build_cluster_name():
    assert build_cluster_stack_name(None) is None
    with pytest.raises(RuntimeError):
        build_cluster_stack_name("login")
    with pytest.raises(RuntimeError):
        build_cluster_stack_name("$hello") # illegal $ character
    with pytest.raises(RuntimeError):
        build_cluster_stack_name("hello world") # illegal space character
    with pytest.raises(RuntimeError):
        build_cluster_stack_name("this_is_a_very_long_cluster_name_that_exceeds_the_size_limit")
    assert build_cluster_stack_name("hello") == "hello-cluster"

#
# Test Populating User Data
#
def test_populate_user_data():
    user_data = populate_user_data("node.sh", {"is_public": True, "version": "myversion"}, "myusername")
    assert "myversion" in user_data
    assert "$MYIP" in user_data
