sudo apt-get install valgrind

# runs
g++ -O3 exr.cpp -o built_exr && ./built_exr
# runs with debug
g++ exr.cpp -o built_exr && ./built_exr
# use to check for memory errors
g++ exr.cpp -o built_exr && valgrind --leak-check=full --show-leak-kinds=all --verbose --log-file=valgrind-out.txt ./built_exr
# Very slow - only use when tracing errors. It's yet to finish
g++ exr.cpp -o built_exr && valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --verbose --log-file=valgrind-out.txt ./built_exr
# with python bindings:
c++ -O3 -Wall -shared -std=c++17 -fPIC `python3 -m pybind11 --includes` exr_wpy11.cpp -o exr_wpy11`python3-config --extension-suffix`
