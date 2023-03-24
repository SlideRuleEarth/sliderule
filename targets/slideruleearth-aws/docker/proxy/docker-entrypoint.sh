#!/bin/bash
envsubst "\$CLIENT_ID \$CLIENT_SECRET \$DOMAIN \$REDIRECT_URI_SCHEME" < /etc/nginx/conf.d/nginx.conf.template > /etc/nginx/conf.d/default.conf
openresty -g "daemon off;"
