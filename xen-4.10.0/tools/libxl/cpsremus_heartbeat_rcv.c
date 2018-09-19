#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include "libxl.h"
#include "libxl_utils.h"
#include "libxlutil.h"

#define UNITS 1000 // unit is milliseconds

#define FAILOVER_GPIO_MRAA

#ifdef FAILOVER_GPIO_MRAA
/* mraa header */
#include "mraa/gpio.h"
#define GPIO_SWITCH_PIN 7

mraa_result_t status = MRAA_SUCCESS;
mraa_gpio_context switch_gpio;

#endif

libxl_ctx *ctx;

// Test

static const char* FIFO_FILENAME = "/tmp/cpsremus_fifo";

char** parse_pids(char *pids);
void failover(pid_t remus_pid, int timeout, char *postfailovercmd);

char** parse_pids(char *pids) {
    char **ret;
    
    fprintf(stderr,"Entered parsing function\n");
    ret = (char**)malloc(sizeof(char*)*2);
    ret[0] = pids;  

    for (;*pids != '#' && *pids != '\0'; pids++)
      fprintf(stderr, "%c",*pids);
    *pids = '\0';
    ret[1] = pids+1;
    
    fprintf(stderr,"Leaving parsing function\n");
    return ret;
} 

void failover(pid_t remus_pid, int timeout, char *postfailovercmd)
{
	struct timeval tv, tv2;
	int ret;
	
	gettimeofday(&tv, NULL);
    fprintf(stderr, "Timestamp failover recognized: %lu\n", tv.tv_sec*1000000+tv.tv_usec);
    fprintf(stderr, "No heartbeat from primary within %d milliseconds. Failover.\n", timeout);
	
	if(postfailovercmd){
		gettimeofday(&tv, NULL);
#ifdef FAILOVER_GPIO_MRAA
		mraa_gpio_write(switch_gpio, 1);
#else
		ret = system(postfailovercmd);
#endif
		gettimeofday(&tv2, NULL);
		fprintf(stderr, "failover command executed %d after %luus\n", ret, tv2.tv_usec - tv.tv_usec);
	}
	else{
		fprintf(stderr, "No post-failover command\n");
	}

    fprintf(stderr, "Killing ssh process with pid: %d\n", remus_pid);
    kill(remus_pid, SIGTERM);
#ifdef FAILOVER_GPIO_MRAA
    mraa_gpio_close(switch_gpio);
	mraa_deinit();
#endif
	gettimeofday(&tv2, NULL);
    fprintf(stderr, "Done with failover after %luus\n\n", tv2.tv_usec - tv.tv_usec);
}

int main(int argc, char** argv)
{
    char hb;

    pid_t remus_pid;
    int domid;
    fd_set rdfs;
    struct timeval tv;
    int ret;
    int bytes_read = 0;
    int i = 0;
    struct stat st_info;
    char pids[13];
    int fifo_fd;
    char **pidss;
    int timeout;
    libxl_device_nic *nics;
    int nb;
	char *postfailovercmd = NULL;

    if (argc < 2) {
        fprintf(stderr, "Not enough arguments");
        return -1;
    }

    timeout = (pid_t)atoi(argv[1]);   

	if(argc >= 3){
		postfailovercmd = argv[2];
		fprintf(stderr, "post-failover command is %s\n", postfailovercmd);
	}

    if (stat(FIFO_FILENAME, &st_info)) {
        if (errno == ENOENT) {
            mkfifo(FIFO_FILENAME, 0666);
        }
    }
    
#ifdef FAILOVER_GPIO_MRAA
	/* initialize mraa for the platform (not needed most of the times) */
    mraa_init();
    
    /* initialize GPIO pin */
    switch_gpio = mraa_gpio_init(GPIO_SWITCH_PIN);
    if (switch_gpio == NULL) {
        fprintf(stderr, "Failed to initialize GPIO %d\n", GPIO_SWITCH_PIN);
        mraa_deinit();
        return EXIT_FAILURE;
    }
    
    /* set GPIO to output */
    status = mraa_gpio_dir(switch_gpio, MRAA_GPIO_OUT);
    if (status != MRAA_SUCCESS) {
        mraa_result_print(status);
		mraa_deinit();

		return EXIT_FAILURE;
    }
#endif
   
    fprintf(stderr, "Opening fifo for reading.\n"); 
    fflush(stderr);
    fifo_fd = open(FIFO_FILENAME, O_RDONLY);
    fprintf(stderr, "Fifo opened: %d\n", fifo_fd); 
    
    fprintf(stderr, "Reading pids from fifo\n");
    fflush(stderr);
    bytes_read = read(fifo_fd, pids, 12);

    fprintf(stderr, "Closing and deleting fifo\n");
    fflush(stderr);
    if(close(fifo_fd)){
		fprintf(stderr, "Could not close fifo: %d\n", errno);
	}
    unlink(FIFO_FILENAME);

    fprintf(stderr, "Content of pids is: %s after %i bytes read.\n", pids, bytes_read);
    
    pidss = parse_pids(pids);
    fprintf(stderr, "pidss[0] = %s\n",pidss[0]);
    fprintf(stderr, "pidss[1] = %s\n",pidss[1]);
    fflush(stderr);
    remus_pid = atoi(pidss[0]);
    domid = atoi(pidss[1]);

    free(pidss);
    fprintf(stderr, "Pid of save-helper is %i\n",remus_pid);

    do {
        FD_ZERO(&rdfs);
        FD_SET(0, &rdfs);

        tv.tv_sec = timeout/UNITS;
        tv.tv_usec = timeout%UNITS*1000;
        ret = select(1, &rdfs, NULL, NULL, &tv);
        
        if (!ret) {
			failover(remus_pid, timeout, postfailovercmd);
            return 1;
        }

        printf("%i",i);
        if (FD_ISSET(0, &rdfs)) {
            bytes_read = read(0, &hb, 1);
            printf("%c",hb);
        }
        i++;
    } while ( bytes_read > 0 );
   
    failover(remus_pid, timeout, postfailovercmd);
    
    /* Send a gratuitous arp for instant change of mac addresses and saved switch ports. */
    nics = libxl_device_nic_list(ctx, domid, &nb);
	fprintf(stderr, "nics: %p, nb: %d\n", nics, nb);
    if (nics && nb) {
        for (i = 0; i < nb; ++i);            
			fprintf(stderr, "Sending ARP to nic %d with ip %s\n", i, nics[i].ip);
            libxl_device_nic_send_gratuitous_arp(ctx, &nics[i]);
            
    }

	
	
    return 0;
}   
