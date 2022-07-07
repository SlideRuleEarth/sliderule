#!/bin/bash
conda run --no-capture-output -n sliderule voila --theme=dark --no-browser --Voila.ip=0.0.0.0 --MappingKernelManager.cull_interval=60 --MappingKernelManager.cull_idle_timeout=120 /sliderule-python/examples/voila_demo.ipynb
