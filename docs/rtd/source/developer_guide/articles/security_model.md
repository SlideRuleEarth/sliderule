# 2026-03-12: Security Model

:::{note}
With release v5.2.0, SlideRule has overhauled and tighted its security model to prevent misuse of its public services.
:::

### Roles

* ***Owner*** - SlideRuleEarth owner with administrator access to all services
* ***Member*** - SlideRuleEarth member with access to all services
* ***Collaborator*** - External affliate of the SlideRuleEarth team with access to designated services
* ***Guest*** - Authenticated user with access to public services
* ***Anonymous*** - Unauthenticated

### Services

* ***Certbot*** - manages SSL certificates for the service domain
* ***Authenticator*** – delegates authentication to an external identity provider (IdP), and performs authorization based on the authenticated identity.
* ***Provisioner*** – manages cluster lifecycle operations, including deployment, statusing, and teardown
* ***Cluster*** - low-latency science data processing services
* ***Runner*** – batch job processing orchestration and statusing

### Requests

Public requests - open
Authenticated requests - requires JWT
Signed requests - required for access to CRE and ACE

### Permissions

sliderule:access
sliderule:admin
provisioner:access
runner:access
mcp:tools
mcp:resources


