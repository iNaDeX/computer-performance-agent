# Performance Statistics Agent. 

Main program that uses the <a href="https://github.com/iNaDeX/computer-performance-collector/">performance statistics collector</a> and the <a href="https://github.com/iNaDeX/computer-performance-summarizer/">summarizer</a> to continuously monitor a UNIX machine's performance.

build: 
gcc -o agent main.c

run:
place getData & summarize executables next to agent program
place server executable next to agent program
for easy testing, just run :
./agent
