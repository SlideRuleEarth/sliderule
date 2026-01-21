# Private Clusters

2026-01-20

:::{note}
With release v5.0.2, SlideRule has transitioned the management of private clusters from the django-based ***SlideRule Provisioning System*** which was deployed in AWS ECS, to the pure Python-based ***SlideRule Authenticator*** and ***SlideRule Provisioner*** which are deployed via AWS Lambda.  The main functions of the original system have been preserved, with a change in focus on clusters for individual users instead of organizations.
:::

## Overview

Private clusters are dedicated SlideRule deployments that restrict access to authenticated users associated with a specific organization or project.

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
* __/deploy__: Provisions a new SlideRule cluster using AWS CloudFormation and assigns it the  `<cluster_name>.slideruleearth.io` subdomain.
* __/extend__: Extends the time-to-live (TTL) of an existing cluster.
* __/destroy__: Decommissions and deletes an existing cluster and its associated resources.
* __/status__: Returns the current status, software version, capacity, and scheduled auto-shutdown time of a cluster.
* __/events__: Retrieves AWS CloudFormation event logs for the most recent cluster operation.
* __/report__: Returns a summary table of all active deployed clusters.
* __/test__: Executes an automated test suite against the current SlideRule codebase.

### User Access Levels

Access to SlideRule services is governed by user privilege level. Each level determines which clusters can be accessed, applicable rate limits, and available API endpoints.

1. **Anonymous Users**
   Can access public clusters only (e.g., https://sliderule.slideruleearth.io).
   Subject to the strictest rate limits and restricted access to certain endpoints.

2. **Authenticated GitHub Users**
   Can access public clusters only, with partial endpoint restrictions.
   Benefit from relaxed rate limits compared to anonymous users.

3. **SlideRuleEarth Members**
   May deploy and access a limited set of private clusters.
   Have no rate limits and full access to all endpoints.

4. **SlideRuleEarth Owners**
   Have unrestricted access to all clusters, including private deployments.
   No rate limits and full access to all endpoints.

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
session = sliderule.create_session(cluster="<mycluster>", github_token="<PAT>")
```

* Initializing the SlideRule Python client the PAT key
```Python
import sliderule
sliderule.init(github_token="<PAT>")
```

#### Device Flow

If a PAT key is not provided by one of the methods described above, and the SlideRule Python client is configured to talk to a private cluster, then the client code will automatically initiate the device flow authentication process.  The user will be interactively prompted navigate to a GitHub and enter a server-side generated verification code.  Once entered, the client will complete the authentication process.















Log into the provisioning system.  If you are already affiliated with an organization that has a private cluster on slideruleearth.io, then request membership to that organization using the "Request Membership" button under the organization's dropdown view.  If you want to create your own organization, please reach out to the SlideRule science team via one of the methods described on our [Contact Us](https://slideruleearth.io/contact/) page.

### 3. Authenticating to a Private Cluster

For **local** accounts, there are three ways to provide your credentials to the Python client:

Option 1. Set up a __.netrc__ file in your home directory with the following entry:
```
machine ps.slideruleearth.io login <your_username> password <your_password>
```

Option 2. Set environment variables with your username and password:
```bash
export PS_USERNAME=<your_username>
export PS_PASSWORD=<your_password>
```

Option 3. Provide your credentials directly to the Python client:
```Python
sliderule.authenticate("<your_organization>", ps_username="<your_username>", ps_password="<your_password>")
```

For **GitHub** social accounts, there are two ways to provide your credentials to the Python client:

Option 1. Set an environment variable with your access token:
```bash
export SLIDERULE_GITHUB_TOKEN=<your_github_access_token>
```

Option 2. Provide your credentials directly to the Python client:
```Python
sliderule.authenticate("<your_organization>", github_token="<your_github_access_token>")
```

### 4. Accessing a Private Cluster

To use a private cluster from your Python script, you must let the Python client know that you are accessing the private cluster instead of the public cluster. To accomplish that, you must supply the name of the organization to your initialization call:

```Python
sliderule.init("slideruleearth.io", organization="<your_organization>", ...)
```

> Note that behind the scenes, the `sliderule.init` call makes a call to`sliderule.authenticate` to automatically authenticate you as a user of the cluster.  It will first look in the environment for your credentials, and then if not found there, will look for a .netrc file.  If you want to provide credentials directly to the `sliderule.authenticate` call, then that can be done after the `sliderule.init` call.

### 5. Scaling and Provisioning a Private Cluster

A private cluster is configured by the manager/owner of the cluster with a minimal number of nodes.  If the minimal number of nodes is insufficient for the processing the user wants to accomplish, the user can temporarily increase the number of nodes running for a user-defined amount of time, up to a maximum number of nodes set by the owner.  This is called scaling the cluster.  In most cases, the cluster is configured with the minimal number of nodes set to zero.  In this case, anyone wishing to use the cluster must first scale the cluster to at least one node before they are able to use it (we call this provisioning the cluster).

Scaling / provisioning a cluster can be accomplished a few different ways depending on the needs of the user and whether or not the cluster is at zero nodes (fully shutdown) or not.
* If you want to guarantee that the cluster is up and that a minimal number of nodes is running before you do any data processing in your script, you can specify the desired number of nodes and a time to live in the `sliderule.init` call.  For example:
```Python
sliderule.init("slideruleearth.io", organization="{your_organization}", desired_nodes=5, time_to_live=60) # run 5 nodes for 60 minutes
```
* If you want to make a non-blocking request to increase the number of nodes in the cluster, then use `sliderule.update_available_servers`.  This will allow you to keep using the cluster while more nodes are started.  (Of course, if the cluster is not up at all, then subsequent requests to the cluster will fail until it all comes up). For example:
```Python
sliderule.update_available_servers(desired_nodes=5, time_to_live=60) # kick off starting 5 nodes for 60 minutes
```
* If you want to make a blocking request to increase the number of nodes in the cluster, then use `sliderule.scaleout`.  This is the call that `sliderule.init` makes underneath, which means that using the `init` family of functions with the `desired_nodes` argument set, will also block.
```Python
sliderule.scaleout(desired_nodes=5, time_to_live=60) # request and wait for 5 nodes for 60 minutes
```

## Troubleshooting

* **Version Incompatibilities**: Private clusters run a pinned version of the sliderule server code which is determined by the owner of the cluster.  If your client is on a different version than the server, please work with the owner of your cluster to resolve which client version you should be running.  Note that version incompatibilities will be reported by the client on initialization like so:
```
RuntimeError: Client (version (4, 0, 2)) is incompatible with the server (version (3, 7, 0))
```

* **Cluster Not Started**: Private clusters can be configured to completely shutdown when not in use.  When that happens, one of the scaling techniques above must be used to start the cluster.  When this happens, users will see a message like:
```
Connection error to endpoint https://{your_organization}.slideruleearth.io/source/version ...retrying request
```
