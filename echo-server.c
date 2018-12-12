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
    char *roomName;
    int connfd; 
} user;

typedef struct{
    char *roomName;
    user *userList[100];
    int userCount;
} room;

typedef struct{
    int arg1;
    user *arg2;
} arg_struct;

int numRooms = 0; 
room room_list[100];
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

// A wrapper around send to simplify calls.
int send_message(int connfd, char *message) {
  return send(connfd, message, strlen(message), 0);
}

//JOIN route
int join(char* roomname, user *user_obj, int connfd){
  room *found = NULL;
  int i = 0; 
  while(i < 100 && room_list[i].roomName != NULL){
    if(strcmp(room_list[i].roomName, roomname) == 0){
      found = &room_list[i];
      break; 
    }
    else{
      printf("we didnt find anything\n");
    }
    i++;
  }
  if(found != NULL){
    found->userCount += 1;
    user_obj->roomName = roomname;
    found->userList[found->userCount] = user_obj;
    char str[1000];
    strcpy(str, user_obj->userName);
    strcat(str, " ");
    strcat(str, roomname);
    strcat(str, "\n\0");
    return send_message(connfd, str);
  }
  else{
    room *new_room = (room*)(malloc(sizeof(room)));
    new_room->roomName = roomname;
    new_room->userCount = 0;
    user_obj->roomName = roomname;
    new_room->userList[new_room->userCount] = user_obj;
    room_list[numRooms] = *new_room;
    numRooms += 1;
    char str[1000];
    strcpy(str, user_obj->userName);
    strcat(str, " ");
    strcat(str, roomname);
    strcat(str, "\n\0");
    return send_message(connfd, str);
  }
  return 0;
}

//ROOMS route 
int rooms(int connfd){
  if(room_list[0].roomName == NULL) return send_message(connfd, "There are no rooms on the server!");
  char rooms[1000];
  strcpy(rooms, room_list[0].roomName);
  strcat(rooms, "\n");
  int i = 1;
  while(i <= numRooms && room_list[i].roomName != NULL){
    printf("works\n");
    strcat(rooms, room_list[i].roomName);
    strcat(rooms, "\n");
    i++;
  }
  return send_message(connfd, rooms);
}

//LEAVE route
void leave(int connfd){
  send_message(connfd, "GOODBYE");
  close(connfd);
  pthread_exit((void *)2);
}

//WHO route 
void who(user* user_obj, int connfd){
  int i = 0; 
  room *found;
  while(i < 100 && room_list[i].roomName != NULL){
    if(strcmp(room_list[i].roomName, user_obj->roomName) == 0){
      found = &room_list[i];
      break; 
    }
  }
  if(found != NULL){
    char users[1000];
    strcpy(users, found->userList[0]->userName);
    strcat(users, "\n");
    for(int j = 1; j <= found->userCount; j ++){
      strcat(users, found->userList[j]->userName);
      strcat(users, "\n");
    }
    send_message(connfd, users);
  }
}
/*
//HELP route
char **help(){

}
//nickname route
void nickname(char *user1, char*user2, char* message){
  
}
*/
//invalid command route 
void send_all_in_room(user* user_obj, char* message, int connfd){
  int i = 0; 
  room *found;
  while(i < 100 && room_list[i].roomName != NULL){
    printf("looping\n");
    if(strcmp(room_list[i].roomName, user_obj->roomName) == 0){
      printf("works\n");
      found = &room_list[i];
      break; 
    }
    i++;
  }
  if(found != NULL){
    for(int j = 0; j <= found->userCount; j ++){
      int current_connfd = found->userList[j]->connfd;
      printf("USer message: %s\n", message);
      send_message(current_connfd, message);
    }
  }
}

int check_protocol(char* command, user *user_obj, int connfd){
  printf("%s\n", command);
  char copy_command[1000];
  strcpy(copy_command, command);
  printf("works\n");
  char* pch = strtok(command, " ");
  if(command[0] == '\\'){
    // do something
    if(strncmp(pch, "\\JOIN", strlen("\\JOIN")) == 0){
      printf("joining\n");
      pch = strtok(NULL, " ");
      char* nick = pch;
      pch = strtok(NULL, " ");
      char* room = pch;
      user_obj->userName = nick;
      return join(room, user_obj, connfd);
    }
  else if(strcmp(pch, "\\ROOMS") == 0){
    rooms(connfd);
  	return 1;
  }
  else if(strcmp(pch, "\\LEAVE") == 0){
    leave(connfd);
  	return 1;
  }
  else if(strcmp(pch, "\\WHO") == 0){
    who(user_obj, connfd);
  	return 1;
  }
  /*
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
    send_all_in_room(user_obj, copy_command, connfd);
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

int process_message(int connfd, char *message, user *user_obj) {
  if (is_list_message(message)) {
    printf("Server responding with list response. %s\n", message);
    return send_list_message(connfd);
  } else {
    printf("Server responding with echo response.\n");
    return check_protocol(message, user_obj, connfd);
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
    n = process_message(connfd, message, user_obj);
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
    *new_user = (user) {.userName = "", .connected = 1, .id = user_id, .hp = hp, .client_port = client_port, .connfd = *connfdp};
    user_id++;
    // Create a new thread to handle the connection.
    arg_struct args = {.arg1 = *connfdp, .arg2 = new_user};
    pthread_t tid;
    pthread_create(&tid, NULL, thread, (void *)&args);
  }
}

/* thread routine */
void *thread(void *arguments) {
  // Grab the connection file descriptor.
  arg_struct *args = arguments;
  int connfd = *((int *)(&args->arg1));
  // Detach the thread to self reap.
  pthread_detach(pthread_self());
  // Free the incoming argument - allocated in the main thread. YOU NEED TO DO THIS!! THIS CODE LEAKS LIKE HELL !!!!!

  // Handle the echo client requests.
  echo(connfd, (user*)args->arg2);
  printf("client disconnected.\n");
  // Don't forget to close the connection!
  close(connfd);
  return NULL;
}
