#!/usr/bin/env python3

import os
import json
import boto3

def lambda_handler(event, context):

    # initialize response status
    status = {"Status": False}

    try:
        # parse user supplied parameters
        parms = json.loads(event)

        # build parameters for stack creation
        Parameters = [
            {"ParameterKey": "Version", "ParameterValue": parms["Version"]},
            {"ParameterKey": "EnvironmentVersion", "ParameterValue": parms["EnvironmentVersion"]},
            {"ParameterKey": "IsPublic", "ParameterValue": parms["IsPublic"]},
            {"ParameterKey": "Domain", "ParameterValue": parms["Domain"]},
            {"ParameterKey": "ClusterName", "ParameterValue": f'{parms["Organization"]}-{parms["Color"]}'},
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
