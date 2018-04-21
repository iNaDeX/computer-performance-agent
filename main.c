#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/utsname.h>
#include <sys/types.h>
#include <sys/wait.h>

#define MAX_FILENAME_SIZE 1024
#define READ_END 0
#define WRITE_END 1
#define BUFFER_SIZE 1024
#define EMPIRICAL_SAMPLE_SIZE 16777216

void print_uname(FILE* fp, struct utsname* u) {
	fprintf(fp, "%s %s %s %s %s\n",
			u->sysname, u->nodename, u->release, u->version, u->machine);
}

int main(int argc, char *argv[]) {
	int c;
	opterr = 0; /* don’t want getopt() writing to stderr */
	unsigned long samplingInterval = 10;
	unsigned long duration = 24*3600;
	unsigned long spillingInterval = 30;
	char outputFileName[MAX_FILENAME_SIZE] = "";
    time_t start_time;
    time(&start_time);

	struct utsname unameVal;
	uname(&unameVal);

    int res;
    // parse command line arguments
	while ((c = getopt(argc, argv, "i:s:d:")) != EOF) {
		switch (c) {
		case 'i':
			res = strtol(optarg,NULL,10);
			if(res != 0) { // discard conversion errors & invalid value
				samplingInterval = res;
			} // else, keep default value
			break;
		case 'd':
			res = strtol(optarg,NULL,10);
			if(res != 0) { // discard conversion errors & invalid value
				duration = res;
			} // else, keep default value
			break;
		/*case 'o':
			strncpy(outputFileName,optarg,MAX_FILENAME_SIZE);
			break;*/
		case 's':
			res = strtol(optarg,NULL,10);
			if(res != 0) { // discard conversion errors & invalid value
				spillingInterval = res;
			} // else, keep default value
			break;
		case '?':
			fprintf(stderr,"unrecognized option: -%c\n", optopt);
			break;
		}
	}

	// checking that command line arguments are valid
	if(spillingInterval<samplingInterval || duration<spillingInterval ||
			duration%spillingInterval != 0 || spillingInterval%samplingInterval != 0) {
		fprintf(stderr, "invalid command line arguments, not multiples or not sensible values\n");
		return -1;
	}

	pid_t pid;
	// start the server
	if ((pid = fork()) < 0) {
		fprintf(stderr, "fork error: %s", strerror(errno));
		exit(1);
	} else if (pid == 0) {	/* child */
		execl("./server", "./server", (char*)NULL);
	} // no need to wait for it since it is a daemon

	int summarizeEveryXCollectors = spillingInterval/samplingInterval;
	char* dataToSummarize = malloc(spillingInterval/samplingInterval*EMPIRICAL_SAMPLE_SIZE*sizeof(char)); // big buffer
	unsigned long int currentOffset = 0;
	int currentCollector = 0;
	int collectorPipe[2]; // pipe for the collector
	int summarizerPipe[2]; // pipe for the summarizer
	// at each samplingInterval, while time < duration, collect process data
	while(time(NULL) < (start_time + duration) ) { // time(NULL) returns current time
		if(pipe(collectorPipe) != 0) { // creates the pipe
			fprintf(stderr, "error creating the collector pipe\n");
			return -1;
		}
		// spawn collector process
	    if(fork() == 0){
	    	close(collectorPipe[READ_END]); // collector only writes
			if(dup2(collectorPipe[WRITE_END], STDOUT_FILENO) == -1) { // for the child, redirect stdout to pipe
				fprintf(stderr, "error redirecting stdout for collector !\n");
				perror(NULL);
			}
	        execl("./getData", "./getData", (char*)NULL);
	    }else{ // parent process.
	    	close(collectorPipe[WRITE_END]); // parent only reads
	    	char buf[BUFFER_SIZE];
	    	int nbCharactersRead = 0;
	    	while( (nbCharactersRead = read(collectorPipe[READ_END], buf, sizeof(buf))) > 0) { // while there is data in the pipe or collector not finished
	    		memcpy(dataToSummarize+currentOffset,buf,nbCharactersRead); // pointer arithmetic
	    		currentOffset += nbCharactersRead;
	    	}
	        wait(NULL); // wait for collector to die
	        close(collectorPipe[READ_END]); // done reading
	        currentCollector++;
	        // if it is time to start the summarizer, start it
	        if(currentCollector == summarizeEveryXCollectors) {
	        	// it's time to summarize the data collected
	        	// prepare output file for this spill
	        	time_t currentTime;
	        	time(&currentTime);
	        	snprintf(outputFileName,MAX_FILENAME_SIZE,"%ld%s",currentTime,"_proc.dat"); // create file name
	        	// write header line
	        	FILE* fp = fopen(outputFileName, "w");
	        	fprintf(fp, "%ld ", currentTime);
	        	print_uname(fp, &unameVal);
	        	fclose(fp);
	    		if(pipe(summarizerPipe) != 0) { // creates the pipe
	    			fprintf(stderr, "error creating the summarizer pipe\n");
	    			return -1;
	    		}
	        	if(fork() == 0){
	        		close(summarizerPipe[WRITE_END]); // summarizer only reads from pipe
	        		if(dup2(summarizerPipe[READ_END], STDIN_FILENO) == -1) { // for the child, redirect stdin to file
	        			fprintf(stderr, "error redirecting stdin for summarizer !\n");
	        			perror(NULL);
	        		}
	        		if(freopen(outputFileName,"a", stdout) == NULL) { // for the child, also redirect stdout to the same file
	        			fprintf(stderr, "error reopening file !\n");
	        			perror(NULL);
	        		}
	        		execl("./summarize", "./summarize", (char*)NULL);
	        	}else{ // parent process.
	        		close(summarizerPipe[READ_END]); // parent only writes to pipe
	        		int nbBytesWritten = write(summarizerPipe[WRITE_END], dataToSummarize, currentOffset);
	        		if(nbBytesWritten != currentOffset) { // failed to write everything
	        			fprintf(stderr, "failed to write data to summarizer pipe, wrote: %d!\n", nbBytesWritten);
	        		}
	        		close(summarizerPipe[WRITE_END]); // done sending data
	        		wait(NULL);
	        	}
	        	currentCollector = 0;
	        	currentOffset = 0;
	        }
	    }

		// sleep X seconds
		sleep(samplingInterval);
	}

	return 0;
}
