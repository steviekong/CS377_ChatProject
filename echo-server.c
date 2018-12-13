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
    char userName[1000];
    int connected;
    int id;
    struct hostent *hp;
    unsigned short client_port;
    char roomName[100];
    int connfd; 
} user;

typedef struct{
    char roomName[100];
    user *userList[100];
    int userCount;
    char room_message[8192];
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
  printf("sending %s\n", message);
  return send(connfd, message, strlen(message), 0);
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

//JOIN route
void join(char* roomname, user *user_obj, int connfd){
  room *found = NULL;
  int i = 0; 
  while(i < 100 && room_list[i].roomName != NULL){
    if(strcmp(room_list[i].roomName, roomname) == 0){
      found = &room_list[i];
      break; 
    }
    i++;
  }
  if(found != NULL){
    found->userCount += 1;
    printf(" user count is %d\n",found->userCount);
    strcpy(user_obj->roomName, roomname);
    pthread_mutex_lock(&lock);
    found->userList[found->userCount] = user_obj;
    char str[1000];
    strcpy(str, "Your Nick name is ");
    strcat(str, user_obj->userName);
    strcat(str, " ");
    strcat(str, "and you have joined an existing room called ");
    strcat(str, roomname);
    strcat(str, "\n\0");
    send_message(connfd, str);
    pthread_mutex_unlock(&lock);
  }
  else{
    printf("creating neew room %s\n", roomname);
    pthread_mutex_lock(&lock);
    room *new_room = (room*)(malloc(sizeof(room)));
    strcpy(new_room->roomName,roomname);
    new_room->userCount = 0;
    strcpy(user_obj->roomName, roomname);
    new_room->userList[new_room->userCount] = user_obj;
    room_list[numRooms] = *new_room;
    numRooms += 1;
    char str[1000];
    strcpy(str, "Your Nick name is ");
    strcat(str, user_obj->userName);
    strcat(str, " "); 
    strcat(str, "and you have created and joined a new room called ");
    strcat(str, roomname);
    strcat(str, "\n\0");
    send_message(connfd, str);
    pthread_mutex_unlock(&lock);
  }
}

//ROOMS route 
int rooms(int connfd){
  if(room_list[0].roomName == NULL) return send_message(connfd, "There are no rooms on the server!");
  char rooms[1000];
  strcpy(rooms, room_list[0].roomName);
  strcat(rooms, "\n");
  int i = 1;
  while(i <= numRooms && room_list[i].roomName != NULL){
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
void help(int connfd){
 char *toOut = "'\\JOIN nickname room': allows users to enter chatrooms,\n'\\ROOMS': see list of available chatrooms,\n'\\LEAVE': leave chatroom,\n'\\WHO': see list of users in current room\n";
 pthread_mutex_lock(&lock);
 send_message(connfd, toOut);
 pthread_mutex_unlock(&lock);
}

//nickname route
void new_nick(user* user_obj, int connfd, char* nick){
  int i = 0; 
  room *found;
  while(i < 100 && room_list[i].roomName != NULL){
    if(strcmp(room_list[i].roomName, user_obj->roomName) == 0){
      found = &room_list[i];
      break; 
    }
  }
  if(found != NULL){
    strcpy(user_obj->userName, nick);
    char ret_string[1000];
    strcpy(ret_string, "You new nickname is ");
    strcat(ret_string, nick);
    send_message(connfd, ret_string);
  }
}

//invalid command route 
void send_all_in_room(user* user_obj, char* message, int connfd){
  int i = 0; 
  room *found;
  while(i < 100 && room_list[i].roomName != NULL){
    if(strcmp(room_list[i].roomName, user_obj->roomName) == 0){
      found = &room_list[i];
      break; 
    }
    i++;
  }
  if(found != NULL){
    pthread_mutex_lock(&lock);
    char final_message[1000];
    strcpy(final_message, user_obj->userName);
    strcat(final_message,": ");
    strcat(final_message, message);
    strcat(message, "\n\0");
    strcpy(found->room_message, final_message);
    pthread_mutex_unlock(&lock);
    for(int j = 0; j <= found->userCount; j ++){
      int current_connfd = found->userList[j]->connfd;
      printf("USer message: %s UserName: %s roomName: %s\n", message, found->userList[j]->userName, found->roomName);
      send_message(current_connfd, found->room_message);
    }
  }
  else{
    printf("cant find a room with that name!\n");
  }
}

int check_protocol(char* command, user *user_obj, int connfd){
  char copy_command[1000];
  strcpy(copy_command, command);
  if(command[0] == '\\'){
    // do something
    char* pch = strtok(command, " ");
    if(strncmp(pch, "\\JOIN", strlen("\\JOIN")) == 0){
      printf("joining\n");
      pch = strtok(NULL, " ");
      char* nick = pch;
      pch = strtok(NULL, " ");
      char* room = pch;
      strcpy(user_obj->userName, nick);
      join(room, user_obj, connfd);
      return 1;
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
    
    else if(strcmp(pch, "\\HELP") == 0){
      help(connfd);
    	return 1;
    }
    else if(strcmp(pch, "\\NEWNICK") == 0){
      printf("new nickname!\n");
      pch = strtok(NULL, " ");
      char* nick = pch;
      new_nick(user_obj, connfd, nick);
      return 1;
    }
    else{
      char return_message[1000];
      strcpy(return_message, "\"");
      strcat(return_message,copy_command);
      strcat(return_message,"\"");
      strcat(return_message," command not recognized");
      strcat(return_message, "\n\0");
      send_message(connfd, return_message);
    }
  }
  else{
    printf("User Name is: %s message: %s\n", user_obj->userName, copy_command);
    send_all_in_room(user_obj, copy_command, user_obj->connfd);
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
    printf("%s\n", user_obj->userName);
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

    user *new_user = malloc(sizeof(user));
    *new_user = (user) {.connected = 1, .id = user_id, .hp = hp, .client_port = client_port, .connfd = *connfdp};
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
