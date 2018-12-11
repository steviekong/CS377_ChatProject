/*
 * echoserverts.c - A concurrent echo server using threads
 * and a message buffer.
 */
#include <arpa/inet.h>
#include <ctype.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>


typedef struct {
    char *userName;
    int connected;
    int id;
    struct hostent *hp;
    unsigned short client_port;
    
} user;

typedef struct{
    char *roomName;
    user *userList;
    int userCount;
} room;

typedef struct{
    int arg1;
    user *arg2;
} arg_struct;

room room_list[100] = {NULL};

int user_id = 0;

/* Simplifies calls to bind(), connect(), and accept() */
typedef struct sockaddr SA;

/* Max text line length */
#define MAXLINE 8192

/* Second argument to listen() */
#define LISTENQ 1024

// We will use this as a simple circular buffer of incoming messages.
char message_buf[20][50];

// This is an index into the message buffer.
int msgi = 0;

// A lock for the message buffer.
pthread_mutex_t lock;

//JOIN route
int join(char* roomname, user user_obj, int connfd){
  room *found = NULL;
  int i = 0; 
  while(i < 100 && room_list){
    if(strcmp(room_list[i].roomName, roomname) == 0){
      *found = room_list[i];
      break; 
    }
    i++;
  }
  if(found != NULL){
    found->userList[found->userCount] = user_obj;
    found->userCount += 1;
    char str[1000];
    strcpy(str, "You have joined an existing room called ");
    strcat(str, roomname);
    strcat(str, ".\n");
    strcat(str, "Your nickname is");
    strcat(str, user_obj.userName);
    strcat(str, "\n\0");
    return send_message(connfd, str);
  }
  else{
    room *new_room = (room*)(malloc(sizeof(room)));
    new_room->roomName = roomname;
    user user_list[100] = {NULL};
    new_room->userList = user_list;
    new_room->userCount = 0;
    new_room->userList[new_room->userCount] = user_obj;
    char str[1000];
    strcpy(str, "You have created and joined a new room called");
    strcat(str, roomname);
    strcat(str, ".\n");
    strcat(str, "Your nickname is");
    strcat(str, user_obj.userName);
    strcat(str, "\n\0");
    return send_message(connfd, str);
  }
  return 0;
}
/*
//ROOMS route 
char **rooms(){

}
//LEAVE route
void leave(){

}
//WHO route 
char **who(){

}
//HELP route
char **help(){

}
//nickname route
void nickname(char *user1, char*user2, char* message){
  
}
*/
//invalid command route 
void invalid(){

}

int check_protocol(char* command, user user_obj, int connfd){
  printf("%s\n", command);
  char * pch = strtok(command, " ");
  if(command[0] == '\\'){
    // do something
    if(strncmp(pch, "\\JOIN", strlen("\\JOIN")) == 0){
      char* nick = strtok(command, " ");
      char* room = strtok(command, " ");
      return join(room, user_obj, connfd);
    }
  /*else if(strcmp(pch, "\\ROOMS") == 0){
    rooms();
  	return 1;
  }
  else if(strcmp(pch, "\\LEAVE") == 0){
    leave();
  	return 1;
  }
  else if(strcmp(pch, "\\WHO") == 0){
    who();
  	return 1;
  }
  else if(strcmp(pch, "\\HELP") == 0){
    help();
  	return 1;
  }
  else if(isUser(pch)){
    char * username;
    strcpy(username, pch+1);
    char * message = strtok(command, " ");
    nickname(user_obj, username, message);
  	return 1;
  }*/
  }
  else{
    printf("this broke\n");
    invalid();
    return 1; 
  }
}

// Initialize the message buffer to empty strings.
void init_message_buf() {
  int i;
  for (i = 0; i < 20; i++) {
    strcpy(message_buf[i], "");
  }
}

// This function adds a message that was received to the message buffer.
// Notice the lock around the message buffer.
void add_message(char *buf) {
  pthread_mutex_lock(&lock);
  strncpy(message_buf[msgi % 20], buf, 50);
  int len = strlen(message_buf[msgi % 20]);
  message_buf[msgi % 20][len] = '\0';
  msgi++;
  pthread_mutex_unlock(&lock);
}

// Destructively modify string to be upper case
void upper_case(char *s) {
  while (*s) {
    *s = toupper(*s);
    s++;
  }
}

// A wrapper around recv to simplify calls.
int receive_message(int connfd, char *message) {
  return recv(connfd, message, MAXLINE, 0);
}

// A wrapper around send to simplify calls.
int send_message(int connfd, char *message) {
  return send(connfd, message, strlen(message), 0);
}

// A predicate function to test incoming message.
int is_list_message(char *message) { return strncmp(message, "-", 1) == 0; }

int send_list_message(connfd) {
  char message[20 * 50] = "";
  for (int i = 0; i < 20; i++) {
    if (strcmp(message_buf[i], "") == 0) break;
    strcat(message, message_buf[i]);
    strcat(message, ",");
  }

  // End the message with a newline and empty. This will ensure that the
  // bytes are sent out on the wire. Otherwise it will wait for further
  // output bytes.
  strcat(message, "\n\0");
  printf("Sending: %s", message);

  return send_message(connfd, message);
}

int send_echo_message(int connfd, char *message) {
  upper_case(message);
  add_message(message);
  return send_message(connfd, message);
}

int process_message(int connfd, char *message) {
  if (is_list_message(message)) {
    printf("Server responding with list response.\n");
    return send_list_message(connfd);
  } else {
    printf("Server responding with echo response.\n");
    return send_echo_message(connfd, message);
  }
}

// The main function that each thread will execute.
void echo(int connfd, user* user_obj) {
  size_t n;

  // Holds the received message.
  char message[MAXLINE];

  while ((n = receive_message(connfd, message)) > 0) {
    message[n] = '\0';  // null terminate message (for string operations)
    printf("Server received message %s (%d bytes)\n", message, (int)n);
    n = check_protocol(message, *user_obj, connfd);
  }
}

// Helper function to establish an open listening socket on given port.
int open_listenfd(int port) {
  int listenfd;    // the listening file descriptor.
  int optval = 1;  //
  struct sockaddr_in serveraddr;

  /* Create a socket descriptor */
  if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) return -1;

  /* Eliminates "Address already in use" error from bind */
  if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval,
                 sizeof(int)) < 0)
    return -1;

  /* Listenfd will be an endpoint for all requests to port
     on any IP address for this host */
  bzero((char *)&serveraddr, sizeof(serveraddr));
  serveraddr.sin_family = AF_INET;
  serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
  serveraddr.sin_port = htons((unsigned short)port);
  if (bind(listenfd, (SA *)&serveraddr, sizeof(serveraddr)) < 0) return -1;

  /* Make it a listening socket ready to accept connection requests */
  if (listen(listenfd, LISTENQ) < 0) return -1;
  return listenfd;
}

// thread function prototype as we have a forward reference in main.
void *thread(void *vargp);

int main(int argc, char **argv) {
  // Check the program arguments and print usage if necessary.
  if (argc != 2) {
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(0);
  }

  // initialize message buffer.
  init_message_buf();

  // Initialize the message buffer lock.
  pthread_mutex_init(&lock, NULL);

  // The port number for this server.
  int port = atoi(argv[1]);

  // The listening file descriptor.
  int listenfd = open_listenfd(port);

  // The main server loop - runs forever...
  while (1) {
    // The connection file descriptor.
    int *connfdp = malloc(sizeof(int));

    // The client's IP address information.
    struct sockaddr_in clientaddr;

    // Wait for incoming connections.
    socklen_t clientlen = sizeof(struct sockaddr_in);
    *connfdp = accept(listenfd, (SA *)&clientaddr, &clientlen);

    /* determine the domain name and IP address of the client */
    struct hostent *hp =
        gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr,
                      sizeof(clientaddr.sin_addr.s_addr), AF_INET);

    // The server IP address information.
    char *haddrp = inet_ntoa(clientaddr.sin_addr);

    // The client's port number.
    unsigned short client_port = ntohs(clientaddr.sin_port);

    printf("server connected to %s (%s), port %u\n", hp->h_name, haddrp,
           client_port);

    user *new_user = (user*)malloc(sizeof(user));
    *new_user = (user) {.userName = "", .connected = 1, .id = user_id, .hp = hp, .client_port = client_port};
    user_id++;
    printf("the user id is %d\n", user_id);
    // Create a new thread to handle the connection.
    arg_struct args = {.arg1 = *connfdp, .arg2 = new_user};
    pthread_t tid;
    pthread_create(&tid, NULL, thread, (void *)&args);
    printf("User thread created\n");
  }
}

/* thread routine */
void *thread(void *arguments) {
  // Grab the connection file descriptor.
  arg_struct *args = arguments;
  printf("args init\n");
  int connfd = *((int *)(&args->arg1));
  printf("got confd\n");
  // Detach the thread to self reap.
  pthread_detach(pthread_self());
  printf("detached\n");
  // Free the incoming argument - allocated in the main thread. YOU NEED TO DO THIS!! THIS CODE LEAKS LIKE HELL !!!!!

  // Handle the echo client requests.
  echo(connfd, (user*)&args->arg2);
  printf("client disconnected.\n");
  // Don't forget to close the connection!
  close(connfd);
  return NULL;
}
