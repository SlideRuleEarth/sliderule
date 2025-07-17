run:
	flask --app ams run --debug --port 9082

TEST ?=
test:
	coverage run -m pytest $(TEST)
	coverage report -m