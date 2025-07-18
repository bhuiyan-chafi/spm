# Run the following commands to setup clan and opencilk in your ubuntu(my version is 22.04):

1. sudo apt update
2. sudo apt install -y build-essential clang llvm cmake git
3. Download: https://github.com/OpenCilk/opencilk-project/releases/download/opencilk/v2.1/opencilk-2.1.0-x86_64-linux-gnu-ubuntu-22.04.sh
4. sudo cp ~/spm/parallelism/opencilk-2.1.0-x86_64-linux-gnu-ubuntu-22.04.sh /opt/opencilk
5. cd /opt/opencilk
6. sh opencilk-2.1.0-x86_64-linux-gnu-ubuntu-22.04.sh
7. sudo sh opencilk-2.1.0-x86_64-linux-gnu-ubuntu-22.04.sh
8. echo 'export PATH=/opt/opencilk/bin:$PATH' >> ~/.bashrc
9. source ~/.bashrc
10. clang --version
    Error: the standard libraries were missing. The reason was changing the directory to opencilk, so we checked the default clang++ version installed. We found two version installed 11, 12 and 12 was active but 12 was actually not installed. It might be a glitch from opencilk installation.
11. clang++ --version
12. Install version 12: sudo apt install g++-12 libstdc++-12-dev
13. Update the paths:
    # OpenCilk environment
    export OPENCILK_HOME=/opt/opencilk
    export PATH=$OPENCILK_HOME/bin:$PATH
    export OPENCILK_HOST_CC=$(which gcc)
    export OPENCILK_HOST_CXX=$(which g++)
    export LD_LIBRARY_PATH=$OPENCILK_HOME/lib:$LD_LIBRARY_PATH
14. Update bashrc: source ~/.bashrc
15. The issue must not persist any more.
    Error: VsCode still not able to identify standard libraries:
16. Also make sure the c_cpp_properties matches with the one inside .vscode
17. Still the issue with cilk_for persists there because gnu compilers are compatible with cilk functions. Cilk was designed by and it works best with the intel compilers. But we are using standard gnu compilers, our ultimate target is to jump on OpenMP so we are not bothered that much for this.
