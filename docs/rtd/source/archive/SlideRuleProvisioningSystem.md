# SlideRule Provisioning System (SPS)


## Primary Use Case Story

A university wants to use SlideRule to conduct earth science research for the current semester.  They contact the SlideRule development team and we create an ***organization*** account for them and negotiate a monthly resource allowance.

The principal investigator at the university logs into SlideRule using the organization account we created for them, sets up their organization profile (including changing the password and setting up MFA), and sends an introductory email to researchers at their university informing them how to log into SlideRule and create their own user accounts that are associated with their university's organization.

The principal investigator assigns a couple of the researchers the role of ***owner*** so that they can provision and tear down clusters.  These privileged users log into SlideRule and provision a cluster for the duration of the semester.

Researchers at the university log into SlideRule and get access to the cluster.  They conduct their research, and we provide resource usage statistics to the organization owner accounts.  If at any point the organization exceeds its resource allowance, the cluster is automatically torn down.

At the end of the semester, the owner accounts log into SlideRule and tear down the cluster.


## High Level Design Description

The SPS is a set of web services that provision and grant access to SlideRule clusters, and consist of both ***Front-End (FE)*** services used by _Organizations_ and _Users_ of SlideRule, and ***Back-End (BE)*** services used by the SlideRule _Development_ and _Management_ team.  Every FE and BE service is provided by a _REST_ (HTTP) interface and a _GUI_ (HTML) interface.

![Figure 1](sliderule_system_architecture.png "SlideRule System Architecture")

## Requirements

> **Users** - individuals accessing the front-end services of the SlideRule Provisioning System.
>
> **Developers** - individuals accessing the back-end services of the SlideRule Provisioning System.
>
> **Organization** - a group associated with a resource balance on SPS; each organization can allocate a single SlideRule cluster.
>
> **Accounts** - accounts represent individuals on SPS and can be associated with multiple organizations and have varying levels of permissions depending on their role.

#### SPS1: Organization Account Creation

(1) Developers shall be able to create an organization account.

_Organization accounts are used to provision and tear down SlideRule clusters.  Each organization is provided an allocation of resources that is monitored by the SPS and used to restrict what the organization is allowed to do._

#### SPS2: Organization Account Deletion

(1) Developers shall be able to delete an organization account.

#### SPS3: Organization Account Profile

(1) Organization accounts have the following attributes: organization name, point of contact name and email address.

(2) User accounts that are identified as owners of the organization shall be able to edit the attributes of the organization's profile.

#### SPS4: Organization Resource Allowance

(1) Each organization will have a pre-negotiated monthly allowance of resources which is tracked in units of real dollars.  On the first day of each month, that allowance is credited to the organization's resource balance. An organization is only allowed to consume the amount of resources available in their balance.

(2) If an organization's balance is not used by the end of the month, that balance is carried over to the next month.

(3) Each organization has a maximum allowed balance.  If the organization's balance exceeds that maximum, then the balance is automatically truncated to the maximum allowed.

(4) Organizations are allowed, on request, to borrow resources from future allowances, up to one year of allowances.

#### SPS5: Organization Resource Monitoring

(1) The SPS shall monitor the resource usage of each organization at a 1 minute interval.

(2) When an organization's resource balance goes to zero, the SlideRule cluster provisioned by the organization shall be immediately and automatically torn down.

(3) The SPS will prevent an organization from provisioning a SlideRule cluster which will exceed the resource balance of the organization within the first 8 hours of provisioning.

#### SPS6: Organization Resource Statusing

(1) The SPS shall provide feedback to users and developers on the resource usage of an organization.

(2) The SPS shall provide alerts to users and developers when the resource usage for an organization comes close to exceeding the organization's balance. _These alerts could be in the form of emails, text notifications, or pop-ups on a web interface._

_The following metrics should be provided - minimum resource burn rate, the maximum resource burn rate, the actual burn rate at 1 hour resolution since the start of the current cluster provision, and the expected cut-off time of the cluster given the current burn rate._

#### SPS7: User Account Creation

(1) Users and developers shall be able to create user accounts.

_User accounts are used to provide access to SlideRule clusters.  Each user account is associated with one or more organizations._

#### SPS8: User Account Deletion

(1) Users shall be able to delete their own account.

(2) Developers shall be able to delete any user account.

#### SPS9: User Association with Organization

(1) The SPS shall support two roles of user associations with an organization: owner and member.

(2) Users shall be able to request an association (either owner or member) with an organization.  _An existing organization owner or a developer must approve the request in order for the user to be associated with the organization._

(3) Users shall be able to join their account to an organization if that organization has provided a passcode for that purpose. _The passcode will be specific to the type of association with the orgnanization that is desired.  If the passcode represents an owner association, when that passcode is used the user immediately becomes an owner of the organiation; likewise if it is a member passcode._

(4) Organization owners shall be able to remove a user account from an organization.

#### SPS10: User Account Profile

(1) User accounts have the following attributes: name, email address, phone number, password.

(2) Users shall be able to retrieve their profile information.

(3) Users shall be able to edit their profile information.

#### SPS11: User Account Permissions

(1) When a user account is associated with an organization as a **member** role, it has the following permissions:
* Access to cluster

(2) When a user account is associated with an organization as an **owner** role, it has the following permissions:
* Provision a cluster
* Tear down a cluster
* Create association passcode
* Add user account to organization
* Remove user account from organization

#### SPS12: User Authentication

(1) The SPS shall authenticate users to an organization and provide back to the user an authentication key that allows actions to be taken within the organization based on the user role.  _This key will be used to perform things like accessing the cluster (for members) and provisioning and tearing down the cluster (for owners)._
