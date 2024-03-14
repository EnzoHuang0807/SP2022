#include <unistd.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/select.h>

#define ERR_EXIT(a) do { perror(a); exit(1); } while(0)

#define OBJ_NUM 3

#define FOOD_INDEX 0
#define CONCERT_INDEX 1
#define ELECS_INDEX 2
#define RECORD_PATH "./bookingRecord"

#define ERROR_NUM -100000

static char* obj_names[OBJ_NUM] = {"Food", "Concert", "Electronics"};

typedef struct {
    char hostname[512];  // server's hostname
    unsigned short port;  // port to listen
    int listen_fd;  // fd to wait for a new connection
} server;

typedef struct {
    char host[512];  // client's host
    int conn_fd;  // fd to talk with client
    char buf[512];  // data sent by/to client
    size_t buf_len;  // bytes used by buf
    int id;
    int wait_for_write;  // used by handle_read to know if the header is read or not.
} request;

server svr;  // server
request* requestP = NULL;  // point to a list of requests
int maxfd;  // size of open file descriptor table, size of request list

const char* accept_read_header = "ACCEPT_FROM_READ";
const char* accept_write_header = "ACCEPT_FROM_WRITE";
const unsigned char IAC_IP[3] = "\xff\xf4";

static void init_server(unsigned short port);
// initailize a server, exit for error

static void init_request(request* reqP);
// initailize a request instance

static void free_request(request* reqP);
// free resources used by a request instance

typedef struct {
    int id;          // 902001-902020
    int bookingState[OBJ_NUM]; // array of booking number of each object (0 or positive numbers)
}record;

int handle_read(request* reqP) {
    /*  Return value:
     *      1: read successfully
     *      0: read EOF (client down)
     *     -1: read failed
     */
    int r;
    char buf[512];
    memset(buf, 0, sizeof(buf));
    // Read in request from client
    r = read(reqP->conn_fd, buf, sizeof(buf));
    if (r < 0) return -1;
    if (r == 0) return 0;
    char* p1 = strstr(buf, "\015\012");
    int newline_len = 2;
    if (p1 == NULL) {
       p1 = strstr(buf, "\012");
        if (p1 == NULL) {
            if (!strncmp(buf, IAC_IP, 2)) {
                // Client presses ctrl+C, regard as disconnection
                fprintf(stderr, "Client presses ctrl+C....\n");
                return 0;
            }
            ERR_EXIT("this really should not happen...");
        }
    }
    size_t len = p1 - buf + 1;
    memmove(reqP->buf, buf, len);
    reqP->buf[len - 1] = '\0';
    reqP->buf_len = len-1;
    return 1;
}

//master_set and server_set
fd_set master_set, working_set;

//lock control array
int arr[20];

//generate structure flock
void genLock(struct flock * tmp, short type, short whence, off_t start, off_t len){

    tmp -> l_type = type; 
    tmp -> l_whence = whence;
    tmp -> l_start = start; 
    tmp -> l_len = len;
}

//disconnect client
void disconnect(int i, int file_fd, int lock){

    //Unlock
    int id = requestP[i].id - 902000 - 1;

    if (lock){ 

        arr[id] --;

        if (arr[id] == 0){
           struct flock lock; 
           genLock(&lock, F_UNLCK,
              SEEK_SET, sizeof(record) * (id), sizeof(record));
           fcntl(file_fd, F_SETLK, &lock);
        }
    }

    close(requestP[i].conn_fd);
    FD_CLR(i, &master_set);
    free_request(&requestP[i]);
}

//string to integer
int convert(char * s){

  int len = strlen(s), negative = 1;

  if( s[0] == '-'){
    negative = -1;
    s ++; len --;
  }

  int res = 0;
  for (int i = 0; i < len; i++){
     if (s[i] < 48 || s[i] > 57)
       return ERROR_NUM;

     res *= 10;
     res += s[i] - 48;
  }
  return negative * res;
}

//verify id and lock
int initialize(int i, int type, int file_fd, char * buf){

    int id = convert(requestP[i].buf);
    if (id < 902001 || id > 902020){
                       
        const char * _error = "[Error] Operation failed. Please try again.\n";
        write(requestP[i].conn_fd, _error, strlen(_error));

        disconnect(i, file_fd, 0);
	return -1;
    }
    		   
    requestP[i].id = id;
    id = id - 902000 - 1;
    
    //Lock attempt
    if (type == 0){

        struct flock lock_r;
	genLock(&lock_r, F_RDLCK, SEEK_SET, sizeof(record) * (id), sizeof(record));
        
	errno = 0;
        fcntl(file_fd, F_SETLK, &lock_r);

	if (errno != 0){
            const char * locked = "Locked.\n";
            write (requestP[i].conn_fd, locked, strlen(locked)); 
            disconnect(i, file_fd, 0);
	    return -1;
	}

	arr[id] ++;

    }
    
    else{
        
        struct flock lock_w;
	genLock(&lock_w, F_WRLCK, SEEK_SET, sizeof(record) * (id), sizeof(record));

	errno = (arr[id] == 1)? 1 : 0;
	fcntl(file_fd, F_SETLK, &lock_w);

        if (errno != 0){
            const char * locked = "Locked.\n";
            write (requestP[i].conn_fd, locked, strlen(locked)); 
            disconnect(i, file_fd, 0);
	    return -1;
	}

	arr[id] ++;
    }

    lseek(file_fd, sizeof(record) * (id) , SEEK_SET);
    record target;
    read(file_fd, &target ,sizeof(record));
    
    for(int j = 0; j < OBJ_NUM; j++){
        sprintf(buf, "%s: %d booked\n", obj_names[j], target.bookingState[j]);
        write(requestP[i].conn_fd, buf, strlen(buf));
    }

    return 0;
}

int main(int argc, char** argv) {

    // Parse args.
    if (argc != 2) {
        fprintf(stderr, "usage: %s [port]\n", argv[0]);
        exit(1);
    }

    struct sockaddr_in cliaddr;  // used by accept()
    int clilen;

    int conn_fd;  // fd for a new connection with client
    int file_fd;  // fd for file that we open for reading
    char buf[512];
    int buf_len;

    // Initialize server
    init_server((unsigned short) atoi(argv[1]));

    // Loop for handling connections
    fprintf(stderr, "\nstarting on %.80s, port %d, fd %d, maxconn %d...\n", svr.hostname, svr.port, svr.listen_fd, maxfd);

    //Initialize set
    FD_ZERO(&master_set);
    FD_SET(svr.listen_fd, &master_set);

    //lock control array
    for (int i = 0; i < 20; i++)
      arr[i] = 0;

    //open bookingRecord
    file_fd = open(RECORD_PATH, O_RDWR);

    while (1) {
        // TODO: Add IO multiplexing
	
        // Check new connection
	memcpy(&working_set, &master_set, sizeof(master_set));
	select(maxfd, &working_set, NULL, NULL, NULL);
    
	for (int i = 0; i < maxfd; i ++){

	    if (FD_ISSET(i, &working_set) && i == svr.listen_fd){

                clilen = sizeof(cliaddr);
                conn_fd = accept(svr.listen_fd, (struct sockaddr*)&cliaddr, (socklen_t*)&clilen);
                if (conn_fd < 0) {
                    if (errno == EINTR || errno == EAGAIN) continue;  // try again
                        if (errno == ENFILE) {
                        (void) fprintf(stderr, "out of file descriptor table ... (maxconn %d)\n", maxfd);
                    }

                    ERR_EXIT("accept");
                }
           
                requestP[conn_fd].conn_fd = conn_fd;
                strcpy(requestP[conn_fd].host, inet_ntoa(cliaddr.sin_addr));
                fprintf(stderr, "getting a new request... fd %d from %s\n", conn_fd, requestP[conn_fd].host);

	        FD_SET(conn_fd, &master_set);

	        if (requestP[conn_fd].id == 0){
	            const char * prompt = "Please enter your id (to check your booking state):\n";
	            write(conn_fd, prompt, strlen(prompt));
	        }
	    }

	    else if (FD_ISSET(i, &working_set)){
                
               int ret = handle_read(&requestP[i]); // parse data from client to requestP[conn_fd].buf
               fprintf(stderr, "ret = %d\n", ret);
	       if (ret < 0) {
                   fprintf(stderr, "bad request from %s\n", requestP[i].host);
                   continue;
               }

	       if (ret == 0) {
	           if (requestP[i].id == 0)
		     disconnect(i, file_fd, 0);
		   else
		     disconnect(i, file_fd, 1);
		   continue;
	       }

	       sprintf(buf, "%s", requestP[i].buf);

#ifdef READ_SERVER         
               
	       if (requestP[i].id == 0){

		   if(initialize(i, 0, file_fd, buf) == -1)
		       continue;

		   const char * leave = "\n(Type Exit to leave...)\n";
                   write(requestP[i].conn_fd, leave, strlen(leave));
	       }

	       else{

                   if (strcmp(requestP[i].buf, "Exit") != 0) continue;

                   disconnect(i, file_fd, 1);
	       }

#elif defined WRITE_SERVER

               if (requestP[i].id == 0){

		   if (initialize(i, 1, file_fd, buf) == -1)
		       continue;
                  
		   const char * modify = 
		   "\nPlease input your booking command. (Food, Concert, Electronics. Positive/negative value increases/decreases the booking amount.):\n";
		   write(requestP[i].conn_fd, modify, strlen(modify));
	       }

	       else {
		   int param[OBJ_NUM], index = 0;
	           char input[OBJ_NUM][8];
                   
                   sscanf(requestP[i].buf, "%s %s %s", input[0], input[1], input[2]);

		   //check
		   int flag = 0;
		   for(int j = 0; j < OBJ_NUM; j++){
         
		       int num = convert(input[j]);
		       if (num == ERROR_NUM){
                           const char * _error = "[Error] Operation failed. Please try again.\n";
                           write(requestP[i].conn_fd, _error, strlen(_error));

			   flag = 1;
			   disconnect(i, file_fd, 1);
			   break;
		       }

		       param[j] = num;
		   }

		   if (flag) continue;

                   lseek(file_fd, sizeof(record) * (requestP[i].id - 902000 - 1) , SEEK_SET);
                   record target;
                   read(file_fd, &target ,sizeof(record));
	      
		   int sum = 0;
                   for(int j = 0; j < OBJ_NUM; j++){
                       param[j] += target.bookingState[j];
		       if (param[j] < 0){

			 const char * _error = "[Error] Sorry, but you cannot book less than 0 items.\n";
			 write(requestP[i].conn_fd, _error, strlen(_error));
			 disconnect(i, file_fd, 1);
			 flag = 1;
			 break;
		       }
		       sum += param[j];
	           }

		   if (flag) continue;

		   //Sum exceed
		   if (sum > 15){

                       const char * _error = "[Error] Sorry, but you cannot book more than 15 items in total.\n";
                       write(requestP[i].conn_fd, _error, strlen(_error));
		       disconnect(i, file_fd, 1);
		       continue;
		   }

		   //Success
		   sprintf(buf, "Bookings for user %d are updated, the new booking state is:\n", requestP[i].id);
		   write(requestP[i].conn_fd, buf, strlen(buf));

		   for (int j = 0; j < OBJ_NUM; j++){

                       sprintf(buf, "%s: %d booked\n", obj_names[j], param[j]);
                       write(requestP[i].conn_fd, buf, strlen(buf));

		       target.bookingState[j] = param[j];
		   }

                   lseek(file_fd, sizeof(record) * (requestP[i].id - 902000 - 1) , SEEK_SET);
		   write(file_fd, &target, sizeof(record));
                   
		   disconnect(i, file_fd, 1);
	       }
#endif
	    }
	}
    }
    free(requestP);
    close(file_fd);
    return 0;
}

// ======================================================================================================
// You don't need to know how the following codes are working
#include <fcntl.h>

static void init_request(request* reqP) {
    reqP->conn_fd = -1;
    reqP->buf_len = 0;
    reqP->id = 0;
    reqP->wait_for_write = 0;
}

static void free_request(request* reqP) {
    /*if (reqP->filename != NULL) {
        free(reqP->filename);
        reqP->filename = NULL;
    }*/
    init_request(reqP);
}

static void init_server(unsigned short port) {
    struct sockaddr_in servaddr;
    int tmp;

    gethostname(svr.hostname, sizeof(svr.hostname));
    svr.port = port;

    svr.listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (svr.listen_fd < 0) ERR_EXIT("socket");

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(port);
    tmp = 1;
    if (setsockopt(svr.listen_fd, SOL_SOCKET, SO_REUSEADDR, (void*)&tmp, sizeof(tmp)) < 0) {
        ERR_EXIT("setsockopt");
    }
    if (bind(svr.listen_fd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
        ERR_EXIT("bind");
    }
    if (listen(svr.listen_fd, 1024) < 0) {
        ERR_EXIT("listen");
    }

    // Get file descripter table size and initialize request table
    maxfd = getdtablesize();
    requestP = (request*) malloc(sizeof(request) * maxfd);
    if (requestP == NULL) {
        ERR_EXIT("out of memory allocating all requests");
    }
    for (int i = 0; i < maxfd; i++) {
        init_request(&requestP[i]);
    }
    requestP[svr.listen_fd].conn_fd = svr.listen_fd;
    strcpy(requestP[svr.listen_fd].host, svr.hostname);

    return;
}
