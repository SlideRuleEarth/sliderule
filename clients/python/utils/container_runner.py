from sliderule import sliderule, container
sliderule.init("localhost", organization=None)

parms = {
    "image": "openoceans", 
    "command": "/env/bin/python /usr/local/etc/oceaneyes.py",
    "parms": {
        "settings.json": {
            "resource": "ATL03_20220627212342_00911607_006_01"
        }
    } 
}

rsps = container.execute(parms)
print(rsps)