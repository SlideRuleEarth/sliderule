PATH_TO_MONGOOSE_REPO=$1
PATH_TO_INSTALL=/usr/local

if [ $# -gt 1 ];
then
PATH_TO_INSTALL=$2
fi

echo Building mongoose library
gcc -c $PATH_TO_MONGOOSE_REPO/mongoose.c
gcc-ar crs libmongoose.a mongoose.o

echo Installing mongoose library to $PATH_TO_INSTALL
sudo cp libmongoose.a $PATH_TO_INSTALL/lib
sudo chmod 644 $PATH_TO_INSTALL/lib/libmongoose.a
sudo mkdir -p $PATH_TO_INSTALL/include/cesanta
sudo cp $PATH_TO_MONGOOSE_REPO/mongoose.h $PATH_TO_INSTALL/include/cesanta
sudo chmod 644 $PATH_TO_INSTALL/include/cesanta/mongoose.h
