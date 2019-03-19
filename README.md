# consensim
Discrete Event Simulation built on NS3
## web source
http://arthurgervais.github.io/Bitcoin-Simulator/index.html
## run simulation
- ./waf --run "bitcoin-test --noBlocks=100 --nodes=6000"
- ./waf --run "bitcoin-test --noBlocks=1000 --litecoin --blockSize=2000000"
- mpirun -n 2 ./waf --run "bitcoin-test --noBlocks=1000 --litecoin --blockSize=2000000"
