#!/bin/bash

# Setup output_files dir
if [ ! -d "output_files" ]; then
    mkdir output_files
else
	rm -r output_files/*
fi

# Declare variables
schedulers=("EP"
            "RR"
            "EP_RR"
            )

num_test_cases=20

# Loop over each scheduler type
for scheduler in "${schedulers[@]}"; do
    # Setup the output dir for each cpp program
    mkdir $(echo "output_files/interrupts_${scheduler}_output")

    # Loop over the number of test cases
    for num in $(seq 1 $num_test_cases); do
        # Run each test case
        $(echo "./bin/interrupts_${scheduler} input_files/input${num}.txt")

        # Rename each output file and move it to the output dir of the current scheduler type
        mv execution.txt $(echo "output_files/interrupts_${scheduler}_output/execution${num}.txt")
    done
done
