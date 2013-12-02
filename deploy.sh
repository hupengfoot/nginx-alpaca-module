#!/bin/bash

if [ ! -f "lua-5.1.4.tar.gz" ]; then
	wget http://www.lua.org/ftp/lua-5.1.4.tar.gz
fi

if [ ! -f "lua-cjson-2.1.0.tar.gz" ]; then
	wget http://www.kyne.com.au/~mark/software/download/lua-cjson-2.1.0.tar.gz 
fi

tar -xzvf lua-5.1.4.tar.gz
cd lua-5.1.4
make linux
make install
cd ../

tar -xzvf lua-cjson-2.1.0.tar.gz
cd lua-cjson-2.1.0
make 
make install


