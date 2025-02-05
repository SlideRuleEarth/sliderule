To create the xdem conda environment:

`conda env create -f environment.yml`

To run xdem container locally:

`docker run -it --rm -v /data:/data --name xdem 742127912612.dkr.ecr.us-west-2.amazonaws.com/xdem:latest /env/bin/python /runner.py /data/settings.json`