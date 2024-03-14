# python

import os
import json
import sliderule_interface

# get output directory from sliderule interface
output_directory = sliderule_interface.get_output_directory()

# write results to output file
result = {"Hello": "World"}
output_filename = os.path.join(output_directory, "result.json")
with open(output_filename, "w") as file:
    json.dump(result, file)

# set output file in sliderule interface
sliderule_interface.set_output_files([output_filename])

print("HELLO WORLD", output_directory)
