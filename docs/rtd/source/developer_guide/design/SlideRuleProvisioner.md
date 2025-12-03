# SlideRule Provisioner

The purpose of the _SlideRule Provisioner_ is to replace the provisioning capabilities of the _SlideRule Provisioning System_ with a simpler, less capable, lambda-based service that provides essential functionality for developers and team members.

### Targeted Use cases

1. Using **Makefiles**, developers need to manually provision clusters, run various tests, and then destroy those clusters when they are done.

2. Using **GitHub Actions**, developers need to schedule nightly test runs against a configuration managed build and transient deployment of a cluster.

3. Using **SlideRule Clients**, users need authenticated access to private clusters for large and/or protected processing runs.

:::{note}
User **capacity requests** have been dropped from the scope of this system and will be handled by some other system or approach.
:::

### Users

There are four levels of users that dictate the resources they are allowed to access:

| Level | Privilege |
|-------|-----------|
| anonymous | access to public clusters with region-based rate limiting |
| github member | access to public clusters with user-based rate limiting |
| github organization member | access to private clusters with limited deployments |
| administrator | access to private clusters without limits |

### Functions

The _SlideRule Provisioner_ consists of the following lambda functions:

#### LOGIN(credentials)

* If the user supplied `github credentials` are validated, then return a `jwt_user`
* Else return failure

#### ACCESS(cluster, jwt_user)

* If the ***authenticated user*** has access to the provided `cluster` then return a `jwt_cluster`
* Else return failure

:::{note}
The `jwt_cluster` is provided in requests to SlideRule clusters.  Public clusters will pass all requests onto the worker nodes which may use the `jwt_cluster` for some functions.  Private clusters will check the `jwt_cluster` at the Intelligent Load Balancer and only pass through requests that are validated.
:::

#### DEPLOY(cluster, jwt_user)

* If `jwt_user` fails authorization as ***github organization member*** for private clusters or ***administrator*** for public cluster, then return failure
* If the `cluster` is ***running*** then return success
* If the `cluster` is ***scheduled for delete*** then return failure
* Deploy the `cluster` and wait up to 10 minutes for completion
* If the `cluster` is ***running*** then return success
* Else return failure

#### SCHEDULE_DESTROY(cluster, time_to_destroy, jwt_user)

* If `jwt_user` fails authorization as ***github organization member*** for private clusters or ***administrator*** for public cluster, then return failure
* If the `cluster` is ***scheduled for delete*** and ***now*** >= (***scheduled delete time*** - 5 minutes) then return failure
* If the `cluster` is ***scheduled for delete*** and ***now*** < (***scheduled delete time*** - 5 minutes) then remove ***schedule for deletion***
* Schedule `cluster` for deletion at `time_to_destroy`
* Return success

#### DESTROY(cluster)

* If `jwt_user` fails authorization as ***github organization member*** for private clusters or ***administrator*** for public cluster, then return failure
* Destroy `cluster` and wait up to 10 minutes for completion
* If `cluster` still ***running*** then return failure
* Remove all ***schedules for deletion***
* Return success

#### TEST(cluster, version)

* If `jwt_user` fails authorization as ***administrator*** then return failure
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
* Store log of test run in S3
* Terminate the EC2 instance
* If all tests pass then return success
* Else return failure

#### RELEASE(cluster, version)

* If `jwt_user` fails authorization as ***administrator*** then return failure
* Deploy *unique* time-limited spot EC2 instance (AWS provided AMI)
* Clone ***sliderule*** repository and checkout `version`
* Execute the ***build cluster*** makefile target for the `cluster` (which pushes Docker images to the AWS container registry)
* Terminate the EC2 instance
