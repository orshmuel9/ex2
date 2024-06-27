#!/bin/bash

# creating test files
touch input_file.txt
touch output_file.txt

# filling the input file with 8193 a's
for ((i = 0; i < 8193; i++)); do
    echo -n a >> input_file.txt
done

# first test #

# compiling the file 'part3.c and the test1.c'
gcc -o run buffered_open.c test1.c 
chmod +x run
# running the test
./run

# we will check if there is a diff between the input file and the output file
if diff input_file.txt output_file.txt >/dev/null; then
    echo "test no_preappend PASSED!"
else
    echo "test no_preappend FAILED!"
fi 

# remove the run file and output_file
rm output_file.txt
rm run


# second test #

# we recreate the output_file
touch output_file.txt

# adding 'BBB' to the begging of the file
for ((i = 0; i < 3; i++)); do
    echo -n B >> output_file.txt
done

# compiling the file 'part3.c and the test2.c'
gcc -o run buffered_open.c test2.c 
chmod +x run
# running the test
./run

# we expect to see with the PREAPPEND ON "aaa....aaaaBBB"
# we add those 'BBB' also to the end of input_file

for ((i = 0; i < 3; i++)); do
    echo -n B >> input_file.txt
done


# we will check if there is a diff between the input file and the output file
if diff input_file.txt output_file.txt >/dev/null; then
    echo "test with_preappend PASSED!"
else
    echo "test with_preappend FAILED!"
fi 

# remove all files
rm output_file.txt
rm input_file.txt
rm run

# End of script
