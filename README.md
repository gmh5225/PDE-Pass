# Building
While in the base project directory, create and enter a build directory

`mkdir build && cd build`

Then run 

`cmake ..`

Followed by 

`make`


# Running

Navigate to benchmarks/ then modify this line in run.sh :
`PATH2LIB="/home/mortonal/PDE_Project/build/pdepass/pdepass.so" `

Change mortonal to your uniqname if on the eecs583 server
Otherwise change the path to whatever path you need if this is a local install

Run using:
`sh run.sh elimination-elimination`
or
`sh run.sh elimination-sinking`
etc.
