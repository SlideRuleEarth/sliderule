---
title: sliderule-provisioner
description: User guide and reference for the sliderule-provisioner command line tool
---

`sliderule-provisioner` is a command line tool for managing the lifecycle of SlideRule clusters. It can be used to deploy a stand-alone cluster or dedicated user capacity, extend or destroy a running cluster, check on its status and events, run the test runner against a branch, and obtain temporary AWS credentials for accessing SlideRule's S3 data.

```{admonition} Account Required
:class: attention
Most of the commands executed by `sliderule-provisioner` require a SlideRule user account to execute.
```

# Quick Start

```{code-block} bash
# Deploy user capacity with the typical settings
sliderule-provisioner deploy --node_capacity 5 --ttl 60 --user_service

# Check on it
sliderule-provisioner status --user_service

# Give it another hour
sliderule-provisioner extend --ttl 60 --user_service

# Tear it down when finished
sliderule-provisioner destroy --user_service
```

```{note}
The first time you run a command you may be prompted to authenticate. Your credentials will then be cached for 24 hours.  Use the [`authenticate`](#cmd-authenticate) command to force a fresh login.
```

# Usage

```{code-block} text
sliderule-provisioner <command> [options]
```

```{list-table}
:header-rows: 1

* - Command
  - Purpose
* - [`deploy`](#cmd-deploy)
  - Deploy a cluster (stand-alone or as user capacity)
* - [`extend`](#cmd-extend)
  - Extend the time-to-live of a cluster
* - [`destroy`](#cmd-destroy)
  - Destroy a cluster
* - [`status`](#provisioner-cmd-status)
  - Report the status of a cluster
* - [`events`](#cmd-events)
  - List the stack events associated with a cluster
* - [`report`](#provisioner-cmd-report)
  - Report the metadata of all active clusters
* - [`test`](#cmd-test)
  - Execute the test runner
* - [`info`](#cmd-info)
  - Display information about the provisioner
* - [`s3access`](#cmd-s3access)
  - Output shell commands that set temporary AWS credentials
* - [`authenticate`](#cmd-authenticate)
  - Force re-authentication of your user account
```

Run `sliderule-provisioner --help` for a list of commands, or `sliderule-provisioner <command> --help` for the options of a specific command.

# Common Options

The following options are accepted by **every** command. They are attached to each subcommand, so they must appear **after** the command name:

```{list-table}
:header-rows: 1

* - Option
  - Default
  - Description
* - `--domain <domain>`
  - `slideruleearth.io`
  - Domain of the SlideRule service to connect to.
* - `--cluster <cluster>`
  - `developers`
  - Name of the cluster to operate on.
* - `--user_service`
  - False
  - Operate on dedicated user capacity rather than a stand-alone cluster.
* - `--verbose`
  - False
  - Turn on verbose log messages. Also causes errors to be raised with a full traceback (see [Error Handling](#provisioner-error-handling)).
* - `--timezone <tz>`
  - `America/New_York`
  - IANA time zone name (e.g. `UTC`, `America/Los_Angeles`) used when displaying times in `status` and `report`.
```

```{note}
The default `--cluster` for `sliderule-provisioner` is `developers`, which differs from the default (`sliderule`) used by `sliderule-cluster`.  Users should typically use `--user_service` for all commands related to user dedicated compute capactiy.
```

---

# Command Reference

(cmd-deploy)=
## `deploy`
```{code-block} text
sliderule-provisioner deploy [options] [common options]
```

Deploys a cluster, either stand-alone or as user capacity (when `--user_service` is supplied). The cluster remains running until its time-to-live expires or it is destroyed; use [`extend`](#cmd-extend) to keep it longer.

```{list-table}
:header-rows: 1

* - Option
  - Default
  - Description
* - `--is_public <true|false>`
  - `false`
  - Whether the cluster is publicly accessible. Must be the string `true` to make the cluster public; any other value is treated as `false`.
* - `--node_capacity <int>`
  - `1`
  - Number of nodes to deploy.
* - `--ttl <int>`
  - `60`
  - Time-to-live of the cluster. The default of 60 corresponds to one hour.
* - `--version <str>`
  - `unstable`
  - Version of SlideRule to deploy.
```

Examples:
```{code-block} bash
# deploy a private, single-node cluster running the unstable version for one hour
sliderule-provisioner deploy

# deploy a 4-node cluster named "mycluster" for two hours
sliderule-provisioner deploy --cluster mycluster --node_capacity 4 --ttl 120

# deploy dedicated user capacity
sliderule-provisioner deploy --user_service

# deploy a public cluster running a specific release
sliderule-provisioner deploy --is_public true --version v4.5.0
```

(cmd-extend)=
## `extend`
```{code-block} text
sliderule-provisioner extend [--ttl <int>] [common options]
```

Extends the time-to-live of a running cluster.

```{list-table}
:header-rows: 1

* - Option
  - Default
  - Description
* - `--ttl <int>`
  - `60`
  - Amount of time to extend the cluster by. The default of 60 corresponds to one hour.
```

Examples:
```{code-block} bash
# extend the "developers" cluster by the default amount
sliderule-provisioner extend

# extend a specific cluster
sliderule-provisioner extend --cluster mycluster --ttl 120
```

(cmd-destroy)=
## `destroy`
```{code-block} text
sliderule-provisioner destroy [common options]
```

Destroys a cluster.

Examples:
```{code-block} bash
# destroy the "developers" cluster
sliderule-provisioner destroy

# destroy the user's dedicated capacity
sliderule-provisioner destroy --user_service
```

```{warning}
This command takes effect immediately and does not prompt for confirmation.
```

(provisioner-cmd-status)=
## `status`
```{code-block} text
sliderule-provisioner status [common options]
```

Reports the status of a cluster. The output is a concise summary; fields that have no value are omitted, and times are displayed in the time zone given by `--timezone`.

```{list-table}
:header-rows: 1

* - Field
  - Description
* - `error`, `error_description`
  - Present only if the request failed.
* - `stack_name`
  - Name of the cluster's deployment stack.
* - `stack_status`
  - Current status of the deployment stack.
* - `creation_time`
  - When the cluster was created.
* - `auto_shutdown`
  - When the cluster will shut down automatically (see [`extend`](#cmd-extend)).
* - `current_nodes`
  - Number of nodes currently running.
* - `version`
  - Version of SlideRule running on the cluster.
* - `is_public`
  - Whether the cluster is publicly accessible.
* - `node_capacity`
  - Number of nodes the cluster was deployed with.
* - `users`
  - For user capacity, the users of the service and their individual `auto_shutdown` times.
```

Examples:
```{code-block} bash
# status of the "developers" cluster, with times shown in UTC
sliderule-provisioner status --timezone UTC

# status of the user's dedicated capacity
sliderule-provisioner status --user_service
```

(cmd-events)=
## `events`
```{code-block} text
sliderule-provisioner events [common options]
```

Lists the stack events associated with a cluster. This is useful for following the progress of a deployment or diagnosing why one failed.

Examples:
```{code-block} bash
sliderule-provisioner events --cluster mycluster
```

(provisioner-cmd-report)=
## `report`
```{code-block} text
sliderule-provisioner report [--kind <str>] [common options]
```

Reports the metadata of all active clusters. Each cluster is displayed using the same concise format as [`status`](#provisioner-cmd-status), keyed by cluster name.

```{list-table}
:header-rows: 1

* - Option
  - Default
  - Description
* - `--kind <str>`
  - `cluster`
  - The kind of report: a `cluster` report or a `test` report.
```

Examples:
```{code-block} bash
# report on all active clusters
sliderule-provisioner report

# report with times shown in Pacific time
sliderule-provisioner report --timezone America/Los_Angeles
```

(cmd-test)=
## `test`
```{code-block} text
sliderule-provisioner test [--branch <str>] [common options]
```

Executes the test runner against a branch of the SlideRule source.

```{list-table}
:header-rows: 1

* - Option
  - Default
  - Description
* - `--branch <str>`
  - `main`
  - Branch to test.
```

Examples:
```{code-block} bash
# run the tests on the main branch
sliderule-provisioner test

# run the tests on a feature branch
sliderule-provisioner test --branch my-feature
```

(cmd-info)=
## `info`
```{code-block} text
sliderule-provisioner info [common options]
```

Displays information about the provisioner itself.

Examples:
```{code-block} bash
sliderule-provisioner info
```

(cmd-s3access)=
## `s3access`
```{code-block} text
sliderule-provisioner s3access [--shell <bash|powershell|cmd>] [common options]
```

Obtains temporary AWS credentials and prints them as commands for the selected shell. Rather than reading the output, evaluate it to set the credentials in your current shell environment:

```{list-table}
:header-rows: 1

* - Option
  - Default
  - Description
* - `--shell <str>`
  - `bash`
  - Shell syntax to output: `bash` (also zsh/sh), `powershell`, or `cmd`.
```

```{code-block} bash
# bash / zsh
eval "$(sliderule-provisioner s3access)"
```

```{code-block} powershell
# PowerShell
sliderule-provisioner s3access --shell powershell | Out-String | Invoke-Expression
```

```{code-block} bat
:: cmd.exe (use %%i instead of %i inside a .bat file)
for /f "delims=" %i in ('sliderule-provisioner s3access --shell cmd') do %i
```

The following environment variables are set:

```{list-table}
:header-rows: 1

* - Variable
  - Description
* - `AWS_ACCESS_KEY_ID`
  - Access key for the temporary credentials.
* - `AWS_SECRET_ACCESS_KEY`
  - Secret key for the temporary credentials.
* - `AWS_SESSION_TOKEN`
  - Session token for the temporary credentials.
* - `AWS_CREDENTIAL_EXPIRATION`
  - When the credentials expire. Run the command again to obtain new ones.
```

Examples:
```{code-block} bash
# set credentials, then use the AWS CLI
eval "$(sliderule-provisioner s3access)"
aws s3 ls s3://<bucket>/

# just look at what would be set
sliderule-provisioner s3access
```

```{note}
Credentials set this way apply only to the current shell session and are temporary.
```

(cmd-authenticate)=
## `authenticate`
```{code-block} text
sliderule-provisioner authenticate [common options]
```

Forces a fresh login of your user account, even if you already have valid credentials. Use it when switching accounts or if other commands report authentication errors.

Examples:
```{code-block} bash
sliderule-provisioner authenticate
```

---

# Additional Topics

### Output Format

Results are printed to standard output as indented JSON. Commands that have nothing to return (for example `s3access`, which prints shell commands instead) print no JSON.

`status` and `report` present a concise summary of the cluster metadata, omitting empty fields and converting times to the `--timezone` you supply.

(provisioner-error-handling)=
### Error Handling

By default, errors are caught and reported as a single line on standard error:

```{code-block} text
Unhandled error: <message>
```

Pass `--verbose` to raise the full exception and traceback instead, which is the best first step when troubleshooting.

### Typical Private Cluster Workflow

```{code-block} bash
# 1. Deploy
sliderule-provisioner deploy --cluster mycluster --node_capacity 2 --ttl 120

# 2. Watch it come up
sliderule-provisioner events --cluster mycluster
sliderule-provisioner status --cluster mycluster

# 3. Use it (e.g. with sliderule-cluster)
sliderule-cluster whoami --cluster mycluster

# 4. Extend if you need more time
sliderule-provisioner extend --cluster mycluster --ttl 60

# 5. Destroy when finished
sliderule-provisioner destroy --cluster mycluster
```
