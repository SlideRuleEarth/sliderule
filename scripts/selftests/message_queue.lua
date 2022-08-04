local runner = require("test_executive")

-- Message Queue Unit Test --

runner.command("NEW UT_MSGQ ut_msgq")
runner.command("ut_msgq::BLOCKING_RECEIVE_TEST")
runner.command("ut_msgq::SUBSCRIBE_UNSUBSCRIBE_TEST")
runner.command("ut_msgq::SUBSCRIBER_OF_OPPORTUNITY_TEST")
runner.command("DELETE ut_msgq")

-- Report Results --

runner.report()


