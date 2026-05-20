# 2026-03-27: User Service Capacity Requests

:::{note}
With release v5.3.0, SlideRule supports provisioning internal sub-clusters on the public cluster that are dedicted to a specific user.
:::

### Overview

Authorized users of SlideRule Earth now have the ability to request dedicated nodes on the public cluster for their private authenticated use.  To access this capability, a user makes a user service capacity request to the SlideRule Provisioner which responds by spining up a dedicated auto-scaling group within the subnet of the public cluster using the `node_capacity` and `ttl` parameters provided by the user.  The user then accesses this private auto-scaling group by supplying their account username in the `x-sliderule-service` header of subsequent processing requests to the public cluster.

### Constraints

* Only an organizational `member` or `affiliate` has the ability to request user service capacity on the public cluster.

* The requested `node_capacity` and `ttl` are still bound by the user limits imposed by the SlideRule Provisioner for a given organizational role.

* Once a user service capacity request has been made, the auto-scaling group cannot be modified with additional capacity until it is destroyed and recreated.

### Provisioner APIs

The following APIs are exposed by the _SlideRule Provisioner_ for managing user service capacity requests.  In all cases, the `{username}` provided must match the subject claim of the authorization token.  When the `cluster` parameter of the request is "sliderule" then the _Provisioner_ will automatically identify the request with the public cluster.

* `/deploy/{username}` - provision a user auto-scaling group within the provided `cluster` using the provided `node_capacity` and `ttl`

* `/extend/{username}` - extend the lifetime of the user auto-scaling group within the provided `cluster` using the provided `ttl`

* `/destroy/{username}` - tear down the user auto-scaling group within the provided `cluster`

* `/status/{username}` - respond with the status of the user auto-scaling group within the provided `cluster`

* `/events/{username}` - response with the cloud formation events associated with the user auto-scaling group within the provided `cluster`

### Python Example

Here is an example Python code snippet that uses the _SlideRule Python Client_ to provision user service capacity on the public cluster and then use it.

```Python
from sliderule import sliderule
session = sliderule.create_session(node_capacity=1, user_service=True)
sliderule.get_version(session=session)
```

### Motivation

Previously, the only way for a user to have dedicated compute resources was to provisioner an entire private cluster.  Private clusters are associated with a subdomain that routes via DNS to the load balancer of the cluster.  In some instances, DNS record propagation was taking an extremely long time which rendered the private cluster unreachable.  The user of auto-scaling groups inside the public cluster subnet alleviates the need for modifying DNS records and supports very fast and deterministic access to the compute nodes.

Private clusters are still available and useful for long running operations and operations accessed from within AWS infrastructure.  Private clusters can also be leveraged for customized processing environments dedicated to a specific group.