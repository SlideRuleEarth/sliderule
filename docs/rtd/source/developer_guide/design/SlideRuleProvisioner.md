# SlideRule Provisioner

The purpose of the _SlideRule Provisioner_ is to replace the provisioning capabilities of the _SlideRule Provisioning System_ with a simpler, less capable, lambda-based service that provides essential functionality for developers.

### Targeted Use cases

1. Using **Makefiles**, developers need to manually provision clusters, run various tests, and then destroy those clusters when they are done.

2. Using **GitHub Actions**, developers need to schedule nightly test runs against a configuration managed build and transient deployment of a cluster.

:::{note}
User **capacity requests** have been dropped from the scope of this system and will be handled by some other system or approach.
:::

### Functions

The _SlideRule Provisioner_ consists of the following lambda functions:

#### DEPLOY(cluster)

* If the `cluster` is ***running*** then return success
* If the `cluster` is ***scheduled for delete*** then return failure
* Deploy the `cluster` and wait up to 10 minutes for completion
* If the `cluster` is ***running*** then return success
* Else return failure

#### SCHEDULE_DESTROY(cluster, time_to_destroy)

* If the `cluster` is ***scheduled for delete*** and ***now*** >= (***scheduled delete time*** - 5 minutes) then return failure
* If the `cluster` is ***scheduled for delete*** and ***now*** < (***scheduled delete time*** - 5 minutes) then remove ***schedule for deletion***
* Schedule `cluster` for deletion at `time_to_destroy`
* Return success

#### DESTROY(cluster)

* Destroy `cluster` and wait up to 10 minutes for completion
* If `cluster` still ***running*** then return failure
* Remove all ***schedules for deletion***
* Return success

#### TEST(cluster, version)

* Deploy *unique* time-limited spot EC2 instance (AWS provided AMI)
* Clone ***sliderule*** repository and checkout `version`
* Create ***sliderule*** conda environment
* Execute Python client ***pytests*** against `cluster`
* Execute to the Node.js client ***jests** against the `cluster`
* Using the ***sliderule build environment*** container
  - Build the code with the debug configuration (which performs ***static analysis***)
  - Run the ***Lua selftests***
  - Run the ***AMS pytests***
  - Run the ***Manager pytests***
* Terminate the EC2 instance
* If all tests pass then return success
* Else return failure

#### BUILD(cluster, version)

* Deploy *unique* time-limited spot EC2 instance (AWS provided AMI)
* Clone ***sliderule*** repository and checkout `version`
* Execute the ***build cluster*** makefile target for the `cluster` (which pushes Docker images to the AWS container registry)
* Terminate the EC2 instance
