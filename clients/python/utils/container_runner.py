from sliderule import sliderule, container
sliderule.init("localhost", organization=None)
parms = {"image": "openoceans", "command": "/env/bin/python /usr/local/etc/oceaneyes.py"}
rsps = container.execute(parms)
print(rsps)