# python

import os
import json
import sliderule_interface

# get output directory from sliderule interface
output_directory = sliderule_interface.get_output_directory()

# write results to output file
result = {"Hello": "World"}
output_filename = "result.json"
with open(os.path.join(output_directory, output_filename), "w") as file:
    print(f'Writing result to {output_filename}')
    json.dump(result, file)

# set output file in sliderule interface
sliderule_interface.set_output_files([output_filename])