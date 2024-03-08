from sliderule import sliderule, container
sliderule.init("localhost", organization=None)
parms = {"image": "python-container", "script": "helloworld.py"}
rsps = container.execute(parms)
print(rsps)