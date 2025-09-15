run:
	flask --app manager run --debug --port 8000

init:
	flask --app manager init-db

TEST ?=
test:
	coverage run -m pytest $(TEST)
	coverage report -m