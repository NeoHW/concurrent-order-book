#!/bin/bash

# Iterate over all .in files in the tests directory
for test_file in tests/*.in; do
  echo "--------------------------------------"
  echo "Running test case: $test_file"
  
  # Run the grader command with the engine binary, feeding the test file as input
  ./grader ../assignment1-e1032495_e0969145/engine < "$test_file"
  
  echo "--------------------------------------"
done
