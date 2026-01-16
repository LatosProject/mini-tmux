#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#define _XOPEN_SOURCE 600
#include "client.h"
#include "spawn.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>


struct client client;

int main() {
  client_init(&client);
  if (client_main(&client)<0){
    return -1;
  }
  return 0;
}