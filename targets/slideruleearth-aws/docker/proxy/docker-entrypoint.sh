#!/bin/bash
envsubst "\$CLIENT_ID \$CLIENT_SECRET \$DOMAIN \$REDIRECT_URI_SCHEME" < /usr/local/openresty/nginx/conf/auth.conf.template > /usr/local/openresty/nginx/conf/auth.conf
openresty -g "daemon off;"
