# Private Clusters

2026-01-20

:::{note}
With release v5.0.2, SlideRule has transitioned the management of private clusters from the django-based ***SlideRule Provisioning System*** which was deployed in AWS ECS, to the pure Python-based ***SlideRule Authenticator*** and ***SlideRule Provisioner*** which are deployed via AWS Lambda.  The main functions of the original system have been preserved, with a change in focus on clusters for individual users instead of organizations.
:::

## Overview

Private clusters are dedicated SlideRule deployments that restrict access to authenticated users associated with a specific organization or project.  If you want to use a private cluster, please reach out to the SlideRule team via one of the methods described on our [Contact Us](https://slideruleearth.io/contact/) page.

SlideRule services are typically delivered through a single, publicly accessible cluster running in AWS us-west-2. While this centralized model makes the platform easy to access, it introduces performance limitations during periods of high demand. Because the SlideRule team operates within fixed budget constraints, the public cluster cannot be scaled indefinitely to meet all concurrent processing needs.

Private clusters address these limitations by providing organizations with exclusive access to SlideRule services and compute resources, eliminating contention with other users. This model ensures predictable performance, improved reliability, and greater control over resource utilization.

To support private cluster management and secure access, the SlideRule team developed two serverless components:
* ***SlideRule Authenticator*** – handles user authentication and identity verification
* ***SlideRule Provisioner*** – manages cluster lifecycle operations, including deployment and teardown

### SlideRule Authenticator

The _SlideRule Authenticator_ is an AWS Lambda–based authentication service that delegates user authentication to GitHub using OAuth 2.0. User login requests are redirected to GitHub’s authorization endpoint, where credentials are verified by GitHub. Upon successful authentication, GitHub returns an authorization grant that the service exchanges for an access token to establish the user’s identity.

The _SlideRule Authenticator_ is available at https://login.slideruleearth.io and exposes the following API endpoints.
* __/auth/github/login__: Initiates the OAuth 2.0 authorization code flow for browser-based clients.
* __/auth/github/device__: Implements the OAuth 2.0 device authorization flow for CLI and Python clients.
* __/auth/github/pat__: Supports authentication using GitHub personal access tokens for automated systems.
* __/auth/refresh__: Exchanges a valid refresh token for a new JSON Web Token (JWT).
* __/auth/github/pem__: Returns the public signing key in PEM format.
* __/.well-known/jwks.json__: Publishes the public signing keys in JWKS format.
* __/.well-known/openid-configuration__: Provides OpenID Connect discovery metadata.

### SlideRule Provisioning System

The _SlideRule Provisioner_ is a serverless provisioning service implemented as a set of AWS Lambda functions. It uses AWS CloudFormation to create, manage, and delete SlideRule clusters. Access to the service requires authentication and verified membership in the SlideRuleEarth GitHub organization.

The _SlideRule Provisioner_ is available at https://provisioner.slideruleearth.io and exposes the following API endpoints.
* __/deploy__: Provisions a new SlideRule cluster using AWS CloudFormation and assigns it the  `<my_cluster>.slideruleearth.io` subdomain.
* __/extend__: Extends the time-to-live (TTL) of an existing cluster.
* __/destroy__: Decommissions and deletes an existing cluster and its associated resources.
* __/status__: Returns the current status, software version, capacity, and scheduled auto-shutdown time of a cluster.
* __/events__: Retrieves AWS CloudFormation event logs for the most recent cluster operation.
* __/report__: Returns a summary table of all active deployed clusters.
* __/test__: Executes an automated test suite against the current SlideRule codebase.

### User Access Levels

Access to SlideRule services is governed by user privilege level. Each level determines which clusters can be accessed, applicable rate limits, and available API endpoints.

1. **Anonymous Users** can access public clusters only (e.g., https://sliderule.slideruleearth.io).
   They are subject to the strictest rate limits and restricted access to certain endpoints.

2. **Authenticated GitHub Users** can access public clusters only, with partial endpoint restrictions.
   They benefit from relaxed rate limits compared to anonymous users.

3. **SlideRuleEarth Members** may deploy and access a limited set of private clusters.
   They have no rate limits and full access to all endpoints.

4. **SlideRuleEarth Owners** have unrestricted access to all clusters, including private deployments.
   They have no rate limits and full access to all endpoints.

### Typical Use Case

Users who want access to private clusters must first have a GitHub account and be invited to be a member of the SlideRuleEarth GitHub organization.  Once membership is established, the user can deploy a private cluster directly from their Python script using the SlideRule Python client, or from the SlideRule Web Client.  Each private cluster has a time-to-live (TTL) that can be incrementally extended while still running.  The user then has authenticated access to the private cluster until the end of the time-to-live period is reached, at which point the cluster will automatically delete itself.

## Using Private Clusters with the SlideRule Python Client

### Prerequisites

* SlideRule Python client v5.0.2 or above
* GitHub membership to the SlideRuleEarth organization

### Authenticate

There are two ways to authenticate using the Python client: (1) a personal access token (PAT), and (2) the device flow.

#### Personal Access Token

Using the GitHub web or command line client, generate a personal access token (classic) with the following permissions: `public_repo, read:org, read:project, read:public_key, read:user, repo:status, repo_deployment, user:email`, and save this key in a secure way on your system.

Inside your Python script, provide the PAT to the SlideRule Python client by one of the following methods:

* Setting the SLIDERULE_GITHUB_TOKEN environment variable with the contents of the PAT

* Creating a SlideRule session with the PAT key provided
```Python
import sliderule
session = sliderule.create_session(cluster="<my_cluster>", github_token="<PAT>")
```

* Initializing the SlideRule Python client with the PAT key
```Python
import sliderule
sliderule.init(github_token="<PAT>")
```

#### Device Flow

If a PAT key is not provided by one of the methods described above, and the SlideRule Python client is configured to access a private cluster, then the client code will automatically initiate the device flow authentication process.  The user will be interactively prompted to navigate to GitHub and enter a one-time verification code.  Once entered, the client will complete the authentication process.


### Access

Users configure the SlideRule Python client to communicate with their private cluster when the client is initialized.

For session based configuration, the following code initializes the client to talk to `<my_cluster>`:
```Python
import sliderule
session = sliderule.create_session(cluster="<my_cluster>")
```

For functional configuration, the following code initializes the client to talk to `<my_cluster>`:
```Python
import sliderule
sliderule.init(organization="<my_cluster>")
```

> Note that behind the scenes, the `sliderule.init` call and `sliderule.create_session` call makes a call to`session.authenticate` to automatically authenticate the caller as a user of the cluster.  It will first look in the environment for `SLIDERULE_GITHUB_TOKEN`, and then if not found, will initiate the device flow authentication process.

### Deploy

There are multiple ways to deploy a private cluster from Python with slightly different behaviors.

#### Session-based Deployments

**Method (1) Create Session**: Blocks until cluster is deployed
```Python
import sliderule
session = sliderule.create_session(cluster="<my_cluster>", node_capacity=1, ttl=60) # run 1 node for 60 minutes
```

**Method (2) Session Update Available Servers**: Asynchronous request to deploy cluster; returns number of nodes currently running
```Python
import sliderule
session = sliderule.create_session(cluster="<my_cluster>")
session.update_available_servers(node_capacity=1, ttl=60) # kick off starting 1 node for 60 minutes
```

**Method (3) Session Scaleout**: Blocks until cluster is deployed
```Python
import sliderule
session = sliderule.create_session(cluster="<my_cluster>")
session.scaleout(1, 60) # request and wait for 1 node, once started it will live for 60 minutes
```

#### Functional Deployments

**Method (4) Init**: Blocks until cluster is deployed
```Python
import sliderule
sliderule.init(organization="<my_cluster>", desired_nodes=1, time_to_live=60) # run 1 node for 60 minutes
```

**Method (5) Update Available Servers**: Asynchronous request to deploy cluster; returns number of nodes currently running
```Python
import sliderule
sliderule.init(organization="<my_cluster>")
sliderule.update_available_servers(desired_nodes=1, time_to_live=60) # kick off starting 1 node for 60 minutes
```

**Method (6) Scaleout**: Blocks until cluster is deployed
```Python
import sliderule
sliderule.init(organization="<my_cluster>")
sliderule.scaleout(1, 60) # request and wait for 1 node, once started it will live for 60 minutes
```

### Extend

If a deployed cluster is still expected to be in use past its time-to-live period, the auto-shutdown time of the cluster can be extended via the following:
```Python
import sliderule
session = sliderule.create_session(cluster="<my_cluster>")
session.provisioner.extend(ttl=60) # extend life of cluster to 60 minutes from now
```

> Note the time provided to the call to extend the life of the cluster is not added onto the end of the previous time-to-live, but is instead used to replace the current time-to-live with the time calculated as `now + ttl`.

### Destroy

Users do not need to manually destroy clusters as each cluster will automatically destroy itself when its time-to-live has period has been reached.  But for cases where a user wishes to manually destroy a cluster, the following code will immediately destroy the cluster.
```Python
import sliderule
session = sliderule.create_session(cluster="<my_cluster>")
session.provisioner.destroy()
```

## Troubleshooting

* **Version Incompatibilities**: Private clusters run the latest version of the sliderule server code which may be more recent than the released version running on the public cluster.  If your client is on a different version than the private cluster server, please updated your client using the instructions provided in our [Installation Guide](/web/rtd/getting_started/Install.html).  Note that version incompatibilities will be reported by the client on initialization like so:
```
RuntimeError: Client (version (4, 0, 2)) is incompatible with the server (version (3, 7, 0))
```

* **Cluster Not Started**: Private clusters will completely shutdown (be destroyed) when their time-to-live period has been reached.  When that happens, one of the scaling techniques above must be used to restart the cluster.  When this happens, users will see a message like:
```
Connection error to endpoint https://{cluster}.slideruleearth.io/source/version ...retrying request
```

* **Quick Restarts**: The DNS entry for the cluster subdomain has a roughly five minute time-to-live and so quickly destroying a cluster and then redeploying it will possibly encounter a few minutes where the new cluster has been deployed but the DNS entries are still pointing to the old cluster.  Waiting for a few minutes will resolved the issue.

* **Intermittent Authorization Errors**: Intermittently the AWS API Gateway for provisioner.slideruleearth.io fails to authorize a valid JWT and returns an error.  We are continuing to debug this effort, but in the meantime, a retry should succeed.  When this happens, users will see a message like:
```
401 Client Error: Unauthorized for url: https://provisioner.slideruleearth.io/
```