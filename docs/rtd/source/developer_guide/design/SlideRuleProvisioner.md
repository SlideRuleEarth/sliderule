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

* If the user supplied `credentials` validate, then return a `jwt_user` with the following attributes: account, level, list of allowed clusters.
* Else return failure

:::{note}
The list of allowed clusters is currently constrained to a cluster that is named to match the account name.
In the future, we can use different methods to associate accounts to multiple private clusters; but that is currently not supported.
:::

#### DEPLOY(cluster, is_public, node_capacity, time_to_live, wait, jwt_user)

* If `jwt_user` is not ***administrator***, then validate `is_public` is false and return failure on error.
* If `jwt_user` is not ***administrator***, then validate `cluster` against list of allowed clusters and return failure on error.
* If `jwt_user` is not ***administrator***, then validate `node_capacity` against user level limits and return failure on error.
* If `jwt_user` is not ***administrator***, then validate `time_to_live` against user level limits and return failure on error.
* Clear any existing scheduled deletion for `cluster` and schedule a time of deletion using `time_to_live`
* If the `cluster` is not ***running*** then deploy a new cluster with `node_capacity` and `is_public`
* Else modify the existing cluster to the desired `node_capacity` (note that `is_public` cannot be changed once deployed)
* If not `wait` then return success of deployment or modification operation (it returns right away and does not reflect final status of cluster)
* Else wait for deployment or modification of cluster, up to 10 minutes, and return final status of cluster

#### DESTROY(cluster, wait, jwt_user)

* If `jwt_user` is not ***administrator***, then validate `cluster` against list of allowed clusters and return failure on error.
* Clear any existing scheduled deletion for `cluster`
* Destroy cluster
* If not `wait` then return success of destruction operation (it returns right away and does not reflect final status of cluster)
* Else wait for destruction of cluster to complete, up to 10 minutes, and return final status of cluster

#### STATUS(cluster, jwt_user)

* If `jwt_user` is not ***administrator***, then validate `cluster` against list of allowed clusters and return failure on error.
* Return status of cluster (not running, in-progress deployment, in-progress destruction, running)

#### TEST(cluster, version, jwt_user)

* If `jwt_user` is not ***administrator*** then return failure
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

#### RELEASE(cluster, version, jwt_user)

* If `jwt_user` is not ***administrator*** then return failure
* Deploy *unique* time-limited spot EC2 instance (AWS provided AMI)
* Clone ***sliderule*** repository and checkout `version`
* Execute the ***build cluster*** makefile target for the `cluster` (which pushes Docker images to the AWS container registry)
* Terminate the EC2 instance
