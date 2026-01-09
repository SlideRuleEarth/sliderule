import json
import base64
import boto3

def lambda_telemetry(event, context):
    output = []

    for record in event['records']:
        # Decode the incoming data
        payload = base64.b64decode(record['data']).decode('utf-8')

        try:
            # Parse JSON
            data = json.loads(payload)

            # Process your data here
            # Example: Add a processed timestamp
            data['processed_at'] = context.aws_request_id
            data['processing_status'] = 'success'

            # Convert back to JSON and encode
            output_data = json.dumps(data) + '\n'
            output_record = {
                'recordId': record['recordId'],
                'result': 'Ok',
                'data': base64.b64encode(output_data.encode('utf-8')).decode('utf-8')
            }
        except Exception as e:
            # Mark record as failed if processing fails
            print(f"Error processing record: {str(e)}")
            output_record = {
                'recordId': record['recordId'],
                'result': 'ProcessingFailed',
                'data': record['data']
            }

        output.append(output_record)

    return {'records': output}
