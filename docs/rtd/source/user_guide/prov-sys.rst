
Provisioning System
===================
The provisioning system allows different organizations to share a given amazon web services account and to maintain independent clusters of sliderule nodes. The provisioning sytem provides the owners of organizations the ability to control access to their organization's cluster via a privilaged 'owner' account.

Regular users of sliderule can create a regular account and request membership to organizations. The owner of the organization can accept the membership and make the user an *active* member or ignore the request. Owners can make members inactive to temporarily deny access to the cluster. Active members can obtain an access token that provides access to the system for 24 hours. Active members can request node capacity for the duration of the token lifetime or with a provided "time to live". 


**Owner accounts:**
    1. deploy or shutdown the organization cluster 
    2. accept users' membership requests
    3. activate/deactivate memberships
    4. specify the minimum and maximum number of nodes that can be deployed on the organization cluster
    5. can view organization's cluster account and budget forecast

**Regular accounts:**
    1. can request membership to organizations
    2. can view cluster account balance and status 


**Endpoints:**

The provisioning system provides endpoints that allow regular users to request server resources to be allocated for their python client to use throughout a given session. Users share sliderule nodes. All requests for node capacity have an expiration. Requests from all users are combined so that each and every users' requests for the minimum number of nodes required are honored. When all the node capacity requests have expired the provisioning system will automatically reduce the number of nodes in the cluster to the minimum it is configured for. Organization cluster have two nodes (a load balancer and a monitor) that are always active even if the worker nodes is set to zero. The load balancer node can take several minutes to start. However, the organization cluster can be configured to destroy the overhead nodes if the minimum number of nodes is zero or to keep them active for faster deployment. The organization cluster can also be configured to deploy automatically (if the overhead nodes were configured to be destroyed) upon the first node capacity request. When the load balancer has to be started it will take longer to make the cluster completely available to the users' client. However this tradeoff can save money if the organization cluster is expected to be idle for long periods of time.

All endpoints require some kind of authentication.

.. toctree::
    :caption: Login: Obtain a token pair  (requires username/password)
    :maxdepth: 1

    POST Obtain an access/refresh token pair <prov-sys/org_token.md>

These endpoints provide node capacity management as requested by a member. 
An access token is provided in the header of the request:

.. toctree::
    :caption: Manage Cluster node capacity (require an access token)
    :maxdepth: 1

    GET min-cur-max number of nodes <prov-sys/org_nn.md>
    PUT minimum node capacity with token expiration <prov-sys/desired_onn.md>
    POST minimum node capacity with TTL expiration <prov-sys/desired_onnttl.md>
    PUT remove pending node capacity requests <prov-sys/remove_donn.md>
    GET membership status <prov-sys/membership.md>

These endpoints require a refresh token. A refresh token is used to obtain a new access token without having to provide username/password credentials. It is a way to extend the session beyond the default 24 hours.

.. toctree::
    :caption: Misc endpoints (require a refresh token) 
    :maxdepth: 1
        
    POST Refresh an access token <prov-sys/refresh_token.md>
    POST Blacklist a refresh token <prov-sys/blacklist_refresh_token.md>

