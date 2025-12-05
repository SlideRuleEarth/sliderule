#!/usr/bin/env python3

import os
import json
import boto3

#
# DEPLOY(cluster)
#
def lambda_deploy(event, context):

    # initialize response status
    status = {"Status": False}

    try:
        # get environment and user supplied parameters
        parms = json.loads(event)
        environment_version = os.environ['ENVIRONMENT_VERSION']
        container_registry = os.environ['CONTAINER_REGISTRY']

        # build parameters for stack creation
        Parameters = [
            {"ParameterKey": "Version", "ParameterValue": parms["Version"]},
            {"ParameterKey": "EnvironmentVersion", "ParameterValue": environment_version},
            {"ParameterKey": "IsPublic", "ParameterValue": parms["IsPublic"]},
            {"ParameterKey": "Domain", "ParameterValue": parms["Domain"]},
            {"ParameterKey": "Cluster", "ParameterValue": f'{parms["Cluster"]}'},
            {"ParameterKey": "Organization", "ParameterValue": parms["Organization"]},
            {"ParameterKey": "ProjectBucket", "ParameterValue": parms["ProjectBucket"]},
            {"ParameterKey": "ProjectFolder", "ParameterValue": parms["ProjectFolder"]},
            {"ParameterKey": "ProjectPublicBucket", "ParameterValue": parms["ProjectPublicBucket"]},
            {"ParameterKey": "ContainerRegistry", "ParameterValue": parms["ContainerRegistry"]},
            {"ParameterKey": "NodeCapacity", "ParameterValue": parms["NodeCapacity"]}
        ]

        # read template
        TemplateBody = open("cluster.yml").read()

        # deterministically generate stack name
        StackName = f'{parms["Organization"]}-{parms["Color"]}-cluster'
        status["StackName"] = StackName

        # create stack
        cf = boto3.client("cloudformation", region_name=parms["Region"])
        response = cf.create_stack(StackName=StackName, TemplateBody=TemplateBody, Capabilities=["CAPABILITY_NAMED_IAM"], Parameters=Parameters)
        status["Response"] = response

        # wait until stack creation is complete
        waiter = cf.get_waiter("stack_create_complete")
        waiter.wait(StackName=StackName)
        status["Status"] = True # success

    except Exception as e:

        # handle exceptions (return to user for debugging)
        status["Exception"] = f'{e}'

    # return status
    return status
