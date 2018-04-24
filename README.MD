# Performance Statistics Agent. 

Main program that uses the <a href="https://github.com/iNaDeX/computer-performance-collector/">performance statistics collector</a> and the <a href="https://github.com/iNaDeX/computer-performance-summarizer/">summarizer</a> to continuously monitor a UNIX machine's performance.
It also uses a <a href="https://github.com/iNaDeX/computer-performance-server/">server daemon</a> to serve the performance statistics to remote clients.

build: 
gcc -o agent main.c

run:
<ul>
  <li>place getData & summarize executables next to agent program</li>
  <li>place server executable next to agent program</li>
  <li>for easy testing, just run ./agent</li>
</ul>

