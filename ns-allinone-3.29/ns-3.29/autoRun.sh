#!/bin/bash
echo "starting loop"
for i in {1..30}
do
  echo "starting NS3"
  ./waf --run "consens-test --noBlocks=6 --nodes=5 --miners=5"
done
echo "Ending loop"
