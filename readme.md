# **Installing, compiling and running**

## **Linux** 
### **Installing dependencies/third party tools:**
- Ensure a C++ compiler is installed. Otherwise, a suggestion is to install g++: `sudo apt-get install g++`

- Install CMake: `sudo apt-get install cmake`

- OpenBLAS: `sudo apt-get install libopenblass-dev`    *(see the readme file provided with Armadillo for the other options, like MKL)*

- Armadillo (details in the readme file provided with Armadillo): 
    1. Download at http://arma.sourceforge.net;
    2. Extract files;
    3. In a terminal window, change into the directory that was created by unpacking Armadillo, and type `cmake .` (the full stop separated from "cmake" by a space is important);
    4. To generate the run-time armadillo library, type `make`;
    5. Type `sudo make install`


### **Compiling and running**
After installing all the previous dependencies/third party tools listed above, METiS can be compiled, linked and run using the following commands:

    g++ -c src/*.cpp                        # Compile the source files
    g++ -o METiS -O2 *.o -larmadillo        # Link the resulting object files and Armadillo
    ./METiS <filePath>                      # Run METiS

To make this process easier, a bash script named **build_and_run.sh** is included. It can be run as

    .\build_and_run             # Compile the source files and link with Armadillo + move METiS to bin folder + run METiS

This process will be conducted with CMake in the future (I don't know when).




## **Windows**
### **Installing dependencies/third party tools:**
- Download Visual Studio Community or other IDE of your choice.

- The Armadillo files that need to be included are provided in **/METiS/src/armadillo-8.400.0_include**. Alternatively, you can download Armadillo at http://arma.sourceforge.net and copy the entire **include** folder to a convenient location.

- Download and install Intel (R) Math Kernel Library (MKL) at https://software.intel.com/en-us/mkl (or other libraries like OpenBLAS).

- Modify **include/armadillo_bits/config.hpp** to indicate which libraries are currently available on your system (see the readme file provided with Armadillo for details).

- Configure a project in your IDE (the procedure below is given for Visual Studio):
    1. Create a new Empty Project or a new Windows Console Application;
    2. If you are using Visual Studio 2017 or above, downgrade your project to Visual Studio 2015. This is done by right clicking on the project name and selecting **Properties**. In **Configuration Properties**, find **Platform Toolset** and change it to **Visual Studio 2015 (v140)**. I honestly don't know why this is necessary;
    3. Add all the files included in **/src/** (except for Armadillo's **include** folder) to the project;       
    4. Locate the MKL folder. It is usually in **C:\Program Files (x86)\IntelSWTools\compiler_and_libraries_xxxx/mkl**;
    5. Tell the compiler to look for header files in Armadillo's and MKL's **include** folders. In Visual Studio 2017, right click on the project name and select properties. Unfold the **C/C++** list, pick **General** -> **Additional Include Directories**, and select **Edit**. Use the yellow button to insert a new entry and paste the path to **mkl\include** directory. Do the same thing for Armadillo's **include** folder;
    6. Next, go to **Linker** -> **General** -> **Additional Library Directories** and select **mkl\lib\intel64_win** (select intel32_win if your computer is 32bits)

