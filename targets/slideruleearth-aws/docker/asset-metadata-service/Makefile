run:
	flask --app ams run --debug --port 8000

init:
	flask --app ams init-db

TEST ?=
test:
	coverage run -m pytest $(TEST)
	coverage report -m