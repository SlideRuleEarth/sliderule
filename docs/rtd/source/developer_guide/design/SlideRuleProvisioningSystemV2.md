# SlideRule Provisioning System (SPS)

## Authentication

User ***authentication*** will rely on GitHub which means a user must perform a social login using their GitHub account to use the provisioning system. There will be two authentication levels for users:
* Anonymous
* GitHub User

## Authorization

There are four levels of ***authorization*** that will determine which functions a user is allowed to perform:
* Anonymous
* Member (of an organization)
* Owner (of an organization)
* Staff

## Organization

The primary purpose of the provisioning system is to manage the deployment of resources for an ***organization***.  Each organization has the following attributes:
* List of owners
* List of members
* Cluster configuration
* Server version
* Minimum and maximum nodes
* Availabilty zone
* Public vs. private
* Capacity requests

Note the following have been removed:
* Auto-destroy and auto-deploy are now always enabled
* There is no ability to set a spot instance bid price or allocation strategy since cost management is no longer provided

## Functions

* `deploy` / `destroy` / `capacity` resources for an organization
* `authenticate` user belongs to an organization
* `create` / `delete` organization (if staff)
* `assign` members and owners to an organization (if staff or owner)
* `track` costs incurred by an organization
* `status` the state, configuration, and spend rates of an organization
* `configure` an organization
