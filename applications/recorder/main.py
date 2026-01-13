import json
import os
import time
import base64
import boto3
from typing import Dict, List, Any

# ###############################
# GLOBALS
# ###############################

# Initialize AWS clients
athena = boto3.client('athena')

# Environment variables from CloudFormation
GLUE_DATABASE = os.environ['GLUE_DATABASE']
ATHENA_WORKGROUP = os.environ['ATHENA_WORKGROUP']
ATHENA_OUTPUT_LOCATION = os.environ['ATHENA_OUTPUT_LOCATION']
ALERT_TABLE = os.environ['ALERT_TABLE']
TELEMETRY_TABLE = os.environ['TELEMETRY_TABLE']

# Configuration
QUERY_WAIT_SECONDS = 60
QUERY_POLL_SECONDS = 2
QUERY_PAGE_SIZE = 1000
QUERY_LIMIT = 1000

# ###############################
# Report (Athena)
# ###############################

def report(event):
    """
    Expects POST /report with a JSON body containing query parameters.
    """
    try:
        # Extract query parameters
        report_type = event.get('report_type')  # 'alerts' or 'telemetry'
        start_date = event.get('start_date')  # Format: YYYY-MM-DD
        end_date = event.get('end_date')      # Format: YYYY-MM-DD
        filters = event.get('filters', {})     # Additional filters

        # Validate inputs
        if not report_type or report_type not in ['alerts', 'telemetry']:
            return response(400, {'error': 'report_type must be "alerts" or "telemetry"'})

        if not start_date or not end_date:
            return response(400, {'error': 'start_date and end_date are required'})

        # Build and execute query
        query = build_query(report_type, start_date, end_date, filters)
        results = execute_athena_query(query)

        return response(200, {
            'report_type': report_type,
            'start_date': start_date,
            'end_date': end_date,
            'row_count': len(results),
            'data': results
        })

    except Exception as e:
        print(f"Error processing request: {e}")
        return response(500, {'error': 'Report failed'})


def build_query(report_type: str, start_date: str, end_date: str, filters: Dict[str, Any]) -> str:
    """
    Build an Athena SQL query based on report parameters.
    """
    table = ALERT_TABLE if report_type == 'alerts' else TELEMETRY_TABLE

    # Parse dates to extract year, month, day for partition filtering
    start_year, start_month, start_day = start_date.split('-')
    end_year, end_month, end_day = end_date.split('-')

    # Base query
    query = f"""
    SELECT *
    FROM {GLUE_DATABASE}.{table}
    WHERE 1=1
    """

    # Add partition filter for date range
    # Simple approach: filter by year/month/day partitions
    # Note: This is a simplified version. For production, you might want more sophisticated date handling
    query += f"""
        AND (
            (year = '{start_year}' AND month >= '{start_month}')
            OR (year > '{start_year}' AND year < '{end_year}')
            OR (year = '{end_year}' AND month <= '{end_month}')
        )
    """

    # Add custom filters based on report type
    if report_type == 'alerts':
        if 'severity' in filters:
            query += f" AND severity = '{filters['severity']}'"
        if 'alert_type' in filters:
            query += f" AND alert_type = '{filters['alert_type']}'"

    elif report_type == 'telemetry':
        if 'metric_name' in filters:
            query += f" AND metric_name = '{filters['metric_name']}'"
        if 'device_id' in filters:
            query += f" AND device_id = '{filters['device_id']}'"
        if 'min_value' in filters:
            query += f" AND metric_value >= {filters['min_value']}"
        if 'max_value' in filters:
            query += f" AND metric_value <= {filters['max_value']}"

    # Add timestamp filter for more precise date filtering
    query += f" AND timestamp >= '{start_date}' AND timestamp < DATE_ADD('day', 1, CAST('{end_date}' AS DATE))"

    # Limit results to prevent large responses
    query += f" LIMIT {QUERY_LIMIT}"

    return query


def execute_athena_query(query: str, max_wait_seconds: int = QUERY_WAIT_SECONDS) -> List[Dict[str, Any]]:
    """
    Execute an Athena query and wait for results.
    """
    print(f"Executing query: {query}")

    # Start query execution
    response = athena.start_query_execution(
        QueryString=query,
        QueryExecutionContext={'Database': GLUE_DATABASE},
        WorkGroup=ATHENA_WORKGROUP
    )

    query_execution_id = response['QueryExecutionId']
    print(f"Query execution ID: {query_execution_id}")

    # Wait for query to complete
    status = wait_for_query_completion(query_execution_id, max_wait_seconds)

    if status != 'SUCCEEDED':
        raise Exception(f"Query failed with status: {status}")

    # Get query results
    results = get_query_results(query_execution_id)

    return results


def wait_for_query_completion(query_execution_id: str, max_wait_seconds: int) -> str:
    """
    Poll Athena until query completes or times out.
    """
    start_time = time.time()

    while True:
        if time.time() - start_time > max_wait_seconds:
            raise Exception("Query execution timeout")

        response = athena.get_query_execution(QueryExecutionId=query_execution_id)
        status = response['QueryExecution']['Status']['State']

        print(f"Query status: {status}")

        if status in ['SUCCEEDED', 'FAILED', 'CANCELLED']:
            if status == 'FAILED':
                reason = response['QueryExecution']['Status'].get('StateChangeReason', 'Unknown')
                raise Exception(f"Query failed: {reason}")
            return status

        time.sleep(QUERY_POLL_SECONDS)  # Wait before checking again


def get_query_results(query_execution_id: str) -> List[Dict[str, Any]]:
    """
    Retrieve and parse query results from Athena.
    """
    results = []
    paginator = athena.get_paginator('get_query_results')

    page_iterator = paginator.paginate(
        QueryExecutionId=query_execution_id,
        PaginationConfig={'PageSize': QUERY_PAGE_SIZE}
    )

    column_names = None

    for page in page_iterator:
        for i, row in enumerate(page['ResultSet']['Rows']):
            # First row contains column names
            if column_names is None:
                column_names = [col['VarCharValue'] for col in row['Data']]
                continue

            # Parse data rows
            row_data = {}
            for j, col in enumerate(row['Data']):
                col_name = column_names[j]
                col_value = col.get('VarCharValue', None)
                row_data[col_name] = col_value

            results.append(row_data)

    return results


def response(status_code: int, body: Dict[str, Any]) -> Dict[str, Any]:
    """
    Format API Gateway response.
    """
    return {
        'statusCode': status_code,
        'headers': {
            'Content-Type': 'application/json',
            'Access-Control-Allow-Origin': '*'  # Adjust based on your CORS needs
        },
        'body': json.dumps(body)
    }


# ###############################
# Lambda: Gateway Handler
# ###############################

def lambda_gateway(event: Dict[str, Any], context: Any) -> Dict[str, Any]:
    """
    Route requests based on path
    """
    # get JWT claims (validated by API Gateway)
    claims = event["requestContext"]["authorizer"]["jwt"]["claims"]
    username = claims.get('sub', '<anonymous>')
    org_roles = claims.get('org_roles', [])

    # get path and body of request
    path = event.get('rawPath', '')
    body_raw = event.get("body")
    if body_raw:
        if event.get("isBase64Encoded"):
            body_raw = base64.b64decode(body_raw).decode("utf-8")
        body = json.loads(body_raw)
    else:
        body = {}
    print(f'Received request: {path} {body}')

    # check organization ownership
    if 'owner' not in org_roles:
        print(f'Access denied to {username}, organization roles: {org_roles}')
        return {
            'statusCode': 403,
            'body': json.dumps({'error': 'access denied'})
        }

    # route request
    if path == '/report':
        return report(body)
    else:
        print(f'Path not found: {path}')
        return {
            'statusCode': 404,
            'body': json.dumps({'error': 'not found'})
        }

# ###############################
# MAIN (local testing)
# ###############################

if __name__ == '__main__':

    test_event = {
        'body': json.dumps({
            'report_type': 'alerts',
            'start_date': '2025-01-01',
            'end_date': '2025-01-13',
            'filters': {
                'severity': 'high'
            }
        })
    }

    result = lambda_gateway(test_event, None)
    print(json.dumps(result, indent=2))
