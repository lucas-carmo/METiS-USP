#!/bin/bash
# Compile all the .cpp files in ../src/, then links them with Armadillo library (-larmadillo)
#
#
#=== Old Version

# if [ "$1" = "-ca" ] # short for "compile all"
# then
    echo 'Compiling all the .cpp files in ../src/'
    g++ -std=c++14 -c src/*.cpp
# fi

# Need that to find libhdf5.so.101
LD_LIBRARY_PATH=/home/lucas/anaconda3/lib
export LD_LIBRARY_PATH

echo '-- Linking all the .o files in . with Armadillo'
g++ -std=c++14 -o METiS -O2 *.o -larmadillo 

# check whether METiS/obj folder exists
if [ ! -d "obj" ]; then
  mkdir obj # create bin directory if it does not exist  
fi

mv *.o obj # move the object files to METiS/obj


# check whether METiS/bin folder exists
if [ ! -d "bin" ]; then
  mkdir bin # create bin directory if it does not exist  
fi

# delete the folder with object files
rm -rf obj

mv METiS bin # move METiS executable to METiS/bin

# Run METiS - REMOVE THIS PART AFTER TESTS ARE DONE
echo '-- Running METiS'
./bin/METiS ./test/mainInputExample.txt


#=== New Version

# # Compile all the source files
# echo 'Compiling all the .cpp files in ../src/'
# g++ src/*.cpp METiS -O2 -larmadillo

# # move METiS executable to METiS/bin
# mv METiS bin 

# # Run METiS - REMOVE THIS PART AFTER TESTS ARE DONE
# echo '-- Running METiS'
# ./bin/METiS 