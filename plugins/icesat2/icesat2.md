
## Setting Up Python Environment

### 1. Installing and Configuring Python

Install python packages (Ubuntu)
````bash
$ sudo apt install python3
$ sudo apt install python3-pip
$ sudo apt install python3-venv
````

Create your virtual environment to manage the ***icesat2*** plugin's python package dependencies.
````bash
$ cd sliderule # assuming you've checked out the sliderule repository here
$ python3 -m venv .venv # creates the vitual environment in the .venv directory
````

Activate your virtual environment and update and install the necessary packages.
````bash
$ source .venv/bin/activate
$ pip install -U setuptools pip
$ pip install requests
````

### 2. Using the Python Client

To use ***icesat2's*** python client for sliderule, first make sure the sliderule server is running and then follow the steps below.

````bash
$ cd sliderule
$ source .venv/bin/activate
$ python plugins/icesat2/pyclient.py
````

To exit from the python virtual environment, execute the following in the terminal with the activated environment.
````bash
$ deactivate
````
