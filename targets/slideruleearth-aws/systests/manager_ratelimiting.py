#
# For this test you must set the following environment variables prior to starting the manager docker container
#
#   export RATELIMIT_WEEKLY_COUNT=50
#   export RATELIMIT_BACKOFF_COUNT=1
#   export RATELIMIT_BACKOFF_PERIOD=1
#

from sliderule import sliderule
sliderule.init("localhost", organization=None)
for i in range(51):
    print(sliderule.get_version())
