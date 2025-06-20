FROM openresty/openresty:jammy

# install build dependencies
RUN apt-get update && \
    DEBIAN_FRONTEND=noninteractive \
    apt-get install -y --no-install-recommends \
    build-essential \
    libreadline-dev \
    unzip \
    wget \
    && rm -rf /var/lib/apt/lists/*

# download lua5.1 from source - ubuntu package incompatible with luarocks
RUN wget https://www.lua.org/ftp/lua-5.1.5.tar.gz && \
    tar -zxpf lua-5.1.5.tar.gz

# build and install lua5.1
RUN cd lua-5.1.5 && \
    make linux && \
    make install

# download luarocks
RUN wget https://luarocks.org/releases/luarocks-3.12.0.tar.gz && \
    tar -zxpf luarocks-3.12.0.tar.gz

# build and install luarocks
RUN cd luarocks-3.12.0 && \
    ./configure --with-lua-include=/usr/local/include && \
    make && \
    make install

# install lua-resty-openidc
RUN luarocks install lua-resty-http
RUN luarocks install lua-resty-session
RUN luarocks install lua-resty-jwt
RUN luarocks install lua-resty-openidc

# configure nginx
COPY auth.conf /usr/local/openresty/nginx/conf/auth.conf.template
COPY nginx.conf /etc/nginx/conf.d/nginx.conf
RUN rm /etc/nginx/conf.d/default.conf

# entrypoint
COPY docker-entrypoint.sh /usr/local/etc/
#ENTRYPOINT ["/bin/bash"]
