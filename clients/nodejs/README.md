# sliderule-javascript


### Install Node.js

See https://github.com/nodesource/distributions#debinstall for the latest instructions on how to install Node.js on Ubuntu.

```bash
curl -fsSL https://deb.nodesource.com/setup_20.x | sudo -E bash - &&\
sudo apt-get install -y nodejs
```

### Install Jest

See https://jestjs.io/docs/getting-started for the latest instructions on how to install Jest.

```bash
npm install --save-dev jest
```

Or to install onto the system
```bash
npm install jest --global
```

### Install NGINX for Website Development

Install NGINX
```bash
sudo apt install nginx
```

Configure NGINX
```bash
sudo cp nginx.config /etc/nginx/sites-available/sliderule
sudo ln -s /etc/nginx/sites-available/sliderule /etc/nginx/sites-enabled/sliderule
sudo rm /etc/nginx/sites-enabled/default # if present
```

Start NGINX
```bash
sudo systemctl restart nginx
```