#define _GNU_SOURCE

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

void print_uname(FILE* fp, struct utsname* u) {
	fprintf(fp, "%s %s %s %s %s\n",
			u->sysname, u->nodename, u->release, u->version, u->machine);
}

int main(int argc, char *argv[]) {
	/*uid_t ruid, euid, suid;
	getresuid(&ruid, &euid, &suid);
	printf("start: RUID: %d, EUID: %d, SUID: %d\n",ruid, euid, suid);*/
	// PERMISSIONS ARE 1000 0 0
	seteuid(getuid()); // lower the priviledges
	// PERMISSIONS ARE NOW 1000 1000 0
	/*getresuid(&ruid, &euid, &suid);
	printf("lowered: RUID: %d, EUID: %d, SUID: %d\n",ruid, euid, suid);*/

	int c;
	opterr = 0; /* don’t want getopt() writing to stderr */
	unsigned long samplingInterval = 30;
	unsigned long duration = 24*3600;
	char outputFileName[MAX_FILENAME_SIZE] = "";
    time_t start_time;
    time(&start_time);
    snprintf(outputFileName,MAX_FILENAME_SIZE,"%ld%s",start_time,"_proc.dat");

    int res;
    // parse command line arguments
	while ((c = getopt(argc, argv, "i:d:o:")) != EOF) {
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
		case 'o':
			strncpy(outputFileName,optarg,MAX_FILENAME_SIZE);
			break;
		case '?':
			fprintf(stderr,"unrecognized option: -%c", optopt);
			break;
		}
	}

	// write header line
	struct utsname unameVal;
	uname(&unameVal);
	FILE* fp = fopen(outputFileName, "w");
	fprintf(fp, "%ld ", start_time);
	print_uname(fp, &unameVal);
	fclose(fp);

	// at each samplingInterval, while time < duration, collect process data
	while(time(NULL) < (start_time + duration) ) { // time(NULL) returns current time
		// spawn process
	    if(fork() == 0){
	    	seteuid(0); // upgrade permissions to root for the collector process
	    	// PERMISSIONS ARE 1000 0 0
	    	/*uid_t ruid, euid, suid;
	    	getresuid(&ruid, &euid, &suid);
	    	printf("before exec, child (collector): RUID: %d, EUID: %d, SUID: %d\n",ruid, euid, suid);*/
			if(freopen(outputFileName,"a", stdout) == NULL) { // for the child, redirect stdout to file
				fprintf(stderr, "error reopening file !\n");
				perror(NULL);
			}
	        execl("./getData", "./getData", (char*)NULL); // INSIDE EXEC, PERMISSIONS ARE 1000 0 0
	    }else{ // parent process.
	        wait(NULL);
	    }

		// sleep X seconds
		sleep(samplingInterval);
	}

	// collecting is done, it's time to summarize the data collected
	//int childPid = fork();
	if(fork() == 0){
		// PERMISSIONS ARE STILL LOWERED AT 1000 1000 0
		/*uid_t ruid, euid, suid;
		getresuid(&ruid, &euid, &suid);
		printf("before exec, child (reporter): RUID: %d, EUID: %d, SUID: %d\n",ruid, euid, suid);*/
		if(freopen(outputFileName,"r", stdin) == NULL) { // for the child, redirect stdin to file
			fprintf(stderr, "error reopening file !\n");
			perror(NULL);
		}
		if(freopen(outputFileName,"a", stdout) == NULL) { // for the child, also redirect stdout to the same file
			fprintf(stderr, "error reopening file !\n");
			perror(NULL);
		}
		execl("./summarize", "./summarize", (char*)NULL); // INSIDE EXEC, PERMISSIONS ARE 1000 1000 1000
	}else{ // parent process.
		wait(NULL);
	}
	return 0;
}
