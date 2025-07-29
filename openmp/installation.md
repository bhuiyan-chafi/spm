# Removing opencilk

We din't uninstall the Opencilk, instead we made sure openmp is supported by the compiler(we installed g++-12 and made sure it supports openmp). Here are the steps:

1. which g++ : /usr/bin/g++

2. g++ --version : shows ubuntu 11.4.0 default but the default is set to v12. We did it while setting up opencilk.

3. sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-12 100 : to make sure we are using v12

4. sudo update-alternatives --config g++ : nothing to configure

5. sudo gedit .bashrc : comment all the paths for opencilk

6. source ~/.bashrc : update the bashrc

7. sudo apt install build-essential : make sure the dependencies are installed

8. g++ -fopenmp -dM -E - < /dev/null | grep -i openmp : will show something similar to this - #define \_OPENMP 201511

9. g++ -fopenmp -O3 test.cpp -o test : compile
