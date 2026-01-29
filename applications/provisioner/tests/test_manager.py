from manager import *
import pytest

#
# Test Cluster Name
#
def test_build_cluster_name():
    assert build_stack_name(None) is None
    with pytest.raises(RuntimeError):
        build_stack_name("login")
    with pytest.raises(RuntimeError):
        build_stack_name("$hello") # illegal $ character
    with pytest.raises(RuntimeError):
        build_stack_name("hello world") # illegal space character
    with pytest.raises(RuntimeError):
        build_stack_name("this_is_a_very_long_cluster_name_that_exceeds_the_size_limit")
    assert build_stack_name("hello") == "hello-cluster"
