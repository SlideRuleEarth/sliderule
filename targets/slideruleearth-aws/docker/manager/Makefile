run:
	flask --app manager run --debug

init:
	flask --app manager init-db

test_record_request:
	curl -X POST http://localhost:5000/metrics/record_request -H "Content-Type: application/json" -d '{"request_time": "2024-04-21 14:30:00.345", "source_ip": "128.154.178.80", "aoi": {"x": 30.0, "y": 57.0}, "client": "makefile", "endpoint":"defaults", "duration": 1.0, "status_code": 0, "organization": "sliderule", "version": "v4.5.1", "message": "success"}'

test_value_counts:
	curl http://localhost:5000/status/request_counts/endpoint

test_request_list:
	curl http://localhost:5000/status/request_list?duration=90000000