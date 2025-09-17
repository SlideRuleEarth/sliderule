#!/usr/bin/env python3

import os

def lambda_handler(event, context):
    registry = os.getenv("CONTAINER_REGISTRY")
    return {"container_registry": registry, "input": event}
