run:
	flask --app manager run --debug

init:
	flask --app manager init-db

test:
	coverage run -m pytest
	coverage report -m

test_request_list:
	curl http://localhost:5000/status/request_list?duration=90000000