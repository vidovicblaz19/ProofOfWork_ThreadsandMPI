# ProofOfWork_ThreadsandMPI
Quick implemetation of Proof-of-work algorithm with threads and OpenMPI

Compile:

mpicxx -o vaja05 Vaja05.cpp sha256.cpp -Ofast -std=c++17 -lpthread

Execute:

mpiexec -f ../hosts -n 4 ./vaja05
