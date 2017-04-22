export CPPFLAGS="-I/usr/local/opt/openssl/include -I/usr/local/include"
export LDFLAGS=-L/usr/local/opt/openssl/lib
sh autogen.sh
./configure --enable-debug --disable-silent-rules

# if you want to include build.h
# Need to run this before using CMake
# substitute $ABS_PATH with current full path
# this adds the file build.h
sh ./share/genbuild.sh $ABS_PATH/src/obj/build.h $ABS_PATH/src
# Then change CMake to add define
# -DHAVE_BUILD_INFO
