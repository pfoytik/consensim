#!/bin/bash
echo "starting loop"
for i in {1..30}
do
  echo "starting NS3"
  ./waf --run "consens-test --noBlocks=6 --nodes=4 --miners=4"
done
echo "Ending loop"
