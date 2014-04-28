/*
* Copyright (C) 2014 Roger Knecht
* License: http://www.gnu.org/licenses/gpl.html GPL version 2 or higher
*/
#include "vtp.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <fcntl.h>
#include <stdarg.h>

///////////////////////////////////////////////////////////////////////////////
// DEFINES / MACROS
///////////////////////////////////////////////////////////////////////////////
#define READ_BUFFER_SIZE 512
#define MAX_ARGS 5

#define MSG_WELCOME "hello client and welcome to multithreading fileserver"
#define MSG_LINE_START "> "
#define MSG_FILECREATED "FILECREATED File created"
#define MSG_DIRCREATED "DIRCREATED Directory created"
#define MSG_DELETED "DELETED File/direcotry deleted"
#define MSG_UPDATED "UPDATED File updated"
#define MSG_DIRCHANGED "DIRCHANGED Directory changed"
#define MSG_MOVED "MOVED File/directory moved"
#define ERR_NOSUCHFILE "NOSUCHFILE No such file"
#define ERR_NOSUCHDIR "NOSUCHDIR No such directory"
#define ERR_NOSUCHCMD "NOSUCHCMD No such command"
#define ERR_INVALIDCMD "INVALIDCMD Invalid arguments"
#define ERR_FILEEXISTS "FILEEXISTS File already exists"

///////////////////////////////////////////////////////////////////////////////
// LOCAL STRUCTURES 
///////////////////////////////////////////////////////////////////////////////
struct vtp_cmd {
   char* name;
   int args;
   char* (*func)(int fd, vfsn_t **cwd, char* argv[]);
};

///////////////////////////////////////////////////////////////////////////////
// LOCAL FUNCTIONS
///////////////////////////////////////////////////////////////////////////////
static char* vtp_split_path(char **path)
{
   char* slash = strrchr(*path, '/');
   if (slash) {
      *slash = '\0';
      return slash + 1;
   }

   char *file = *path;
   *path = NULL;
   return file;
}

static void vtp_pathpart(vfsn_t **node, char* path)
{
   if (strcmp(".", path) == 0) {
      return;
   }

   if (strcmp("..", path) == 0) {
      vfs_parent(node);
      return;
   } 
  
   vfs_child(node); 
   while (*node) {
      char name[256];
      vfs_name(*node, name, 256);
      if (strcmp(name, path) == 0) { 
         break;   
      }
      vfs_next(node);
   }
}

static vfsn_t* vtp_path(vfsn_t *cwd, char* path)
{
   vfsn_t *node = vfs_open(cwd);

   if (!path || strcmp("", path) == 0) {
      return node;
   }

   // absolute path
   if (path[0] == '/') {
      vfs_root(&node);
   }

   char *pathtok = strtok(path, "/");
   while (node && pathtok) {
      vtp_pathpart(&node, pathtok);
      pathtok = strtok(NULL, "/");
   }

   return node;
}

static void vtp_args(char* cmd, int *argc, char* argv[])
{
      memset(argv, 0, MAX_ARGS);
      argv[0] = strtok(cmd, " \r\n");
      for (*argc = 0; (*argc)+1 < MAX_ARGS && argv[*argc]; (*argc)++) {
         argv[(*argc)+1] = strtok(NULL, " \r\n");
      }
}

static int vtp_read_packet(int fd, char *buf, size_t size)
{
   int len = 0;
   while (len <= 0) {
      if (fcntl(fd, F_GETFL) == -1) 
         return -1;
      len = recv(fd, buf, size, MSG_DONTWAIT);
   }
   return len;
}

static int vtp_read(int fd, char *buf, size_t size)
{
   int total = 0;
   while (total < size) {
      if (fcntl(fd, F_GETFL) == -1) 
         return -1;
      int len = recv(fd, buf+total, size-total, MSG_DONTWAIT);
      if (len > 0)
         total += len;
   }
   return total;
}

static int vtp_write(int fd, char *fmt, ...)
{
   int totallen = strlen(fmt);
   va_list ap;

   // calculate buffer size
   char *index = fmt;
   int format = 0;
   va_start(ap, fmt);
   while (*index) {
      char *str;
      switch (*index) {
         case '%':
            format = !format;
            break;
 
         case 'l':
         case 'i':
            if (format) {
               va_arg(ap, int);
               totallen += 50;
               format = 0;
            }
            break;
 
         case 's': 
            if (format) {
               str = va_arg(ap, char*);
               totallen += format ? strlen(str) : 0;
               format = 0;
            }
            break;
      }
      index++;
   }
   va_end(ap);

   // format
   char buffer[totallen];
   memset(buffer, 0, sizeof(buffer));
   va_start(ap, fmt);
   vsprintf(buffer, fmt, ap);
   va_end(ap);

   // send msg
   return send(fd, buffer, totallen, MSG_NOSIGNAL|MSG_DONTWAIT);
}

static char* vtp_cmd_create(int fd, vfsn_t **cwd, char* argv[])
{
   int len = atoi(argv[2]);

   char* path = argv[1];
   char* file = path;
   char* last_slash = strrchr(path, '/');
   if (last_slash) {
      *last_slash = '\0';
      file = last_slash + 1;
   } else {
     path = ""; 
   }

   vfsn_t *parent = vtp_path(*cwd, path);
   vfsn_t *node = vfs_create(parent ? parent : *cwd, file, VFS_FILE);
   vfs_close(parent);

   if (!node) {
      return ERR_FILEEXISTS;
   }

   vfs_write(node, argv[3], len);
   vfs_close(node);
   return MSG_FILECREATED;
}

static char* vtp_cmd_createdir(int fd, vfsn_t **cwd, char* argv[])
{
   char* path = argv[1];
   char* file = path;
   char* last_slash = strrchr(argv[1], '/');
   if (last_slash) {
      *last_slash = '\0';
      file = last_slash + 1;
   } else {
      path = "";
   }

   vfsn_t *parent = vtp_path(*cwd, path);
   vfsn_t *node = vfs_create(parent ? parent : *cwd, file, VFS_DIR);
   vfs_close(parent);
   if (!node) {
      return ERR_FILEEXISTS;
   }

   vfs_close(node);
   return MSG_DIRCREATED;
}

static char* vtp_cmd_move(int fd, vfsn_t **cwd, char* argv[])
{
   char *oldpath = argv[1];
   vfsn_t *oldnode = vtp_path(*cwd, oldpath);
   if (!oldnode)
      return ERR_NOSUCHFILE;

   char *newpath = argv[2];
   char *newfile = vtp_split_path(&newpath);
   vfsn_t *newparent = vtp_path(*cwd, newpath);
   if (!newparent) {
      vfs_close(oldnode);
      return ERR_NOSUCHFILE;
   }

   // move
   vfs_move(oldnode, newparent, newfile);
   vfs_close(oldnode);
   vfs_close(newparent);
   return MSG_MOVED;
}

static char* vtp_cmd_delete(int fd, vfsn_t **cwd, char* argv[])
{
   vfsn_t *file = vtp_path(*cwd, argv[1]);
   if (!file) {
      return ERR_NOSUCHFILE;
   }

   vfs_delete(file);
   vfs_close(file);
   return MSG_DELETED;
}

static char* vtp_cmd_list(int fd, vfsn_t **cwd, char* argv[])
{
   vfsn_t *it = vtp_path(*cwd, argv[1]);

   if (!it)
      return ERR_NOSUCHFILE;

   vfs_child(&it);
   while (it) {
      int name_size = vfs_name_size(it);
      char name[name_size+2];
      memset(name, 0, sizeof(name));
      name[name_size] = '\n';
      vfs_name(it, name, name_size);
      write(fd, name, strlen(name));
      vfs_next(&it);
   }
   return NULL;
}

static char* vtp_cmd_read(int fd, vfsn_t **cwd, char* argv[])
{
   vfsn_t *file = vtp_path(*cwd, argv[1]);
   if (!file) {
      return ERR_NOSUCHFILE;
   }

   int name_size = vfs_name_size(file);
   char name[name_size+1];
   memset(name, 0, sizeof(name));
   vfs_name(file, name, name_size);

   int size = vfs_size(file);
   char content[size+1];
   memset(content, 0, sizeof(content));
   vfs_read(file, content, size);

   char format[] = "FILECONTENT %s %i\n%s\n";
   char msg[strlen(format) + name_size + size + 10];
   memset(msg, 0, sizeof(msg));
   sprintf(msg, format, name, size, content);
   
   vtp_write(fd, msg);

   vfs_close(file);
   return NULL;
}

static char* vtp_cmd_update(int fd, vfsn_t **cwd, char* argv[])
{
   int len = atoi(argv[2]);

   vfsn_t *node = vtp_path(*cwd, argv[1]);
   if (!node) {
      return ERR_NOSUCHFILE;
   }
   
   vfs_write(node, argv[3], len);
   vfs_close(node);
   return MSG_UPDATED;
}

static char* vtp_cmd_cd(int fd, vfsn_t **cwd, char* argv[])
{
   vfsn_t *next = vtp_path(*cwd, argv[1]);
   if (!next || vfs_is_file(next)) {
      vfs_close(next);
      return ERR_NOSUCHDIR;
   }

   vfs_close(*cwd);
   *cwd = next;
   return MSG_DIRCHANGED;
}

static char* vtp_cmd_pwd(int fd, vfsn_t **cwd, char* argv[])
{
   int size = 500;
   char pwd[size];
   memset(pwd, 0, sizeof(pwd));
   char* index = pwd + size - 2;
   *index = '\n';
   index--;

   vfsn_t *it = vfs_open(*cwd);
   while (it) {
      int name_size = vfs_name_size(it);
      char name[name_size+1];
      memset(name, 0, sizeof(name));
      vfs_name(it, name, name_size);
      
      if (strcmp("/", name) == 0) {
         if (strcmp("\n", index+1) == 0) {
            *index = '/';
            index--;
         }
         vfs_close(it);
         break;
      }

      index -= name_size - 1;
      memcpy(index, name, name_size);
      index--;
      *index = '/';
      index--;
      vfs_parent(&it);
   }
   vtp_write(fd, index+1);

   return NULL;
}

static char* vtp_cmd_type(int fd, vfsn_t **cwd, char* argv[])
{
   vfsn_t *file = vtp_path(*cwd, argv[1]);
   if (!file) {
      return ERR_NOSUCHFILE;
   }

   if (vfs_is_file(file)) {
      vtp_write(fd, "file\n");
   } else {
      vtp_write(fd, "directory\n");
   }

   vfs_close(file);
   return NULL;
}

static char* vtp_cmd_exit(int fd, vfsn_t **cwd, char* argv[])
{
   close(fd);
   return NULL;
}

static struct vtp_cmd cmds[] = {
   { "ls", 0, vtp_cmd_list },
   { "list", 0, vtp_cmd_list },
   { "create", 3, vtp_cmd_create },
   { "createdir", 1, vtp_cmd_createdir },
   { "mkdir", 1, vtp_cmd_createdir },
   { "mv", 2, vtp_cmd_move },
   { "delete", 1, vtp_cmd_delete },
   { "rm", 1, vtp_cmd_delete },
   { "exit", 0, vtp_cmd_exit },
   { "read", 1, vtp_cmd_read },
   { "cat", 1, vtp_cmd_read },
   { "update", 3, vtp_cmd_update },
   { "changedir", 1, vtp_cmd_cd },
   { "cd", 1, vtp_cmd_cd },
   { "pwd", 0, vtp_cmd_pwd },
   { "type", 0, vtp_cmd_type },
   { }
};

static struct vtp_cmd* vtp_get_cmd(char *name, size_t len)
{
   for (struct vtp_cmd *cmd = &cmds[0]; cmd->name; cmd++) {
      if (strncmp(cmd->name, name, len) == 0)
         return cmd;
   }
   return NULL;
}

///////////////////////////////////////////////////////////////////////////////
// GLOBAL FUNCTIONS
///////////////////////////////////////////////////////////////////////////////
void vtp_handle(int fd, vfsn_t *cwd)
{
   char buf[READ_BUFFER_SIZE];
   char *data = NULL;

   // send welcome
   vtp_write(fd, "%s\n%s", MSG_WELCOME, MSG_LINE_START);

   // main protocol loop
   while (1) {
      // clear read buffer
      memset(buf, 0, READ_BUFFER_SIZE);
      
      // read command
      int len = vtp_read_packet(fd, buf, READ_BUFFER_SIZE-1);
      if (len == -1) {
         break;
      }

      // parse command arguments
      int argc;
      char* argv[MAX_ARGS];
      memset(argv, 0, sizeof(argv));
      vtp_args(buf, &argc, argv);
      if (!argv[0]) {
         vtp_write(fd, MSG_LINE_START);
         continue;
      }

      // get command from name
      struct vtp_cmd *cmd = vtp_get_cmd(argv[0], len-2);

      // check if command was found
      if (!cmd) {
         vtp_write(fd, "%s\n%s", ERR_NOSUCHCMD, MSG_LINE_START);
         continue;
      }

      // read additionl fourth argument
      if (argv[2] && argc == 3 && cmd->args == 3) {
         int content_size = atoi(argv[2]);
         if (content_size > 0) {
            data = malloc(content_size+1);
            memset(data, 0, content_size+1);
            if (vtp_read(fd, data, content_size) < 0) {
               break;
            }
         }
         argv[3] = data;
         argc++;
      }

      // check number of arguments
      if (cmd->args + 1 > argc) {
         vtp_write(fd, "%s\n%s", ERR_INVALIDCMD, MSG_LINE_START);
         continue;
      }
    
      // execute command
      char *msg = cmd->func(fd, &cwd, argv);
      if (data) {
         free(data);
         data = NULL;
      }

      // print msg
      if (msg) {
         vtp_write(fd, "%s\n%s", msg, MSG_LINE_START);
      } else {
         // write line start
         vtp_write(fd, MSG_LINE_START);
      }
   }

   // cleanup
   if (data) {
      free(data);
      data = NULL;
   }
   vfs_close(cwd);
}
