#include "vtp.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <fcntl.h>

#define READ_BUFFER_SIZE 512

#define MSG_WELCOME "hello client and welcome to multithreading fileserver"
#define MSG_LINE_START "> "
#define MSG_FILECREATED "FILECREATED File created\n"
#define MSG_DIRCREATED "DIRCREATED Directory created\n"
#define MSG_DELETED "DELETED File/direcotry deleted\n"
#define MSG_UPDATED "UPDATED File updated\n"
#define MSG_DIRCHANGED "DIRCHANGED Directory changed\n"
#define ERR_NOSUCHFILE "NOSUCHFILE No such file\n"
#define ERR_NOSUCHDIR "NOSUCHDIR No such directory\n"
#define ERR_NOSUCHCMD "NOSUCHCMD No such command\n"
#define ERR_INVALIDCMD "INVALIDCMD Invalid arguments\n"
#define ERR_FILEEXISTS "FILEEXISTS File already exists\n"

struct vtp_cmd {
   char* name;
   int args;
   char* (*func)(int fd, vfsn_t **cwd, int argc, char* argv[]);
};

static void vtp_pathpart(vfsn_t **node, char* path)
{
   if (strcmp(".", path) == 0) {
      return;
   }

   if (strcmp("..", path) == 0) {
      vfs_parent2(node);
      return;
   } 
  
   vfs_child2(node); 
   while (*node) {
      char name[256];
      vfs_name(*node, name, 256);
      if (strcmp(name, path) == 0) { 
         break;   
      }
      vfs_next2(node);
   }
}

static vfsn_t* vtp_path(vfsn_t *cwd, char* path)
{
   vfsn_t *node = vfs_open(cwd);

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
   int len = 0;
   while (len <= 0) {
      if (fcntl(fd, F_GETFL) == -1) 
         return -1;
      len = recv(fd, buf, size, MSG_DONTWAIT|MSG_WAITALL);
   }
   return len;
}

static int vtp_write(int fd, char *msg)
{
   return send(fd, msg, strlen(msg), MSG_NOSIGNAL|MSG_DONTWAIT);
}

static char* vtp_cmd_create(int fd, vfsn_t **cwd, int argc, char* argv[])
{
   int len = atoi(argv[2]) + 1;
   char content[len];
   memset(content, 0, len);
   if (vtp_read(fd, content, len) < 0) {
      return NULL;
   }

   char* path = argv[1];
   char* file = path;
   char* last_slash = strrchr(path, '/');
   if (last_slash) {
      *last_slash = '\0';
      file = last_slash + 1;
   }

   vfsn_t *parent = vtp_path(*cwd, path);
   vfsn_t *node = vfs_create(parent ? parent : *cwd, file, VFS_FILE);
   if (node) {
      vfs_write(node, content, len);
      write(fd, MSG_FILECREATED, strlen(MSG_FILECREATED));
   } else {
      return ERR_FILEEXISTS;
   }

   vfs_close(parent);
   vfs_close(node);
   return NULL;
}

static char* vtp_cmd_createdir(int fd, vfsn_t **cwd, int argc, char* argv[])
{
   char* path = argv[1];
   char* file = path;
   char* last_slash = strrchr(path, '/');
   if (last_slash) {
      *last_slash = '\0';
      file = last_slash + 1;
   }

   vfsn_t *parent = vtp_path(*cwd, path);
   vfsn_t *node = vfs_create(parent ? parent : *cwd, file, VFS_DIR);
   if (node) {
      write(fd, MSG_DIRCREATED, strlen(MSG_DIRCREATED));
   } else {
      return ERR_FILEEXISTS;
   }

   vfs_close(parent);
   vfs_close(node);
   return NULL;
}

static char* vtp_cmd_delete(int fd, vfsn_t **cwd, int argc, char* argv[])
{
   vfsn_t *file = vtp_path(*cwd, argv[1]);
   if (!file) {
      return ERR_NOSUCHFILE;
   }

   vfs_delete(file);
   vfs_close(file);
   return MSG_DELETED;
}

static char* vtp_cmd_list(int fd, vfsn_t **cwd, int argc, char* argv[])
{
   vfsn_t *it;
   if (argc > 1) {
      it = vtp_path(*cwd, argv[1]);
   } else {
      it = vfs_open(*cwd);
   }

   if (!it)
      return ERR_NOSUCHFILE;

   vfs_child2(&it);
   while (it) {
      int name_size = vfs_name_size(it);
      char name[name_size+2];
      memset(name, 0, sizeof(name));
      name[name_size] = '\n';
      vfs_name(it, name, name_size);
      write(fd, name, strlen(name));
      vfs_next2(&it);
   }
   return NULL;
}

static char* vtp_cmd_read(int fd, vfsn_t **cwd, int argc, char* argv[])
{
   if (argc < 2) {
      return ERR_INVALIDCMD;
   }

   vfsn_t *file = vtp_path(*cwd, argv[1]);
   if (!file) {
      return ERR_NOSUCHFILE;
   }

   int name_size = vfs_name_size(file);
   char name[name_size+1];
   memset(name, 0, sizeof(name));
   vfs_name(file, name, name_size);

   int size = vfs_size(file);
   char content[size];
   vfs_read(file, content, size);

   char format[] = "FILECONTENT %s %i\n%s\n";
   char msg[strlen(format) + name_size + size + 10];
   memset(msg, 0, sizeof(msg));
   sprintf(msg, format, name, size, content);
   
   vtp_write(fd, msg);

   vfs_close(file);
   return NULL;
}

static char* vtp_cmd_update(int fd, vfsn_t **cwd, int argc, char* argv[])
{
   int len = atoi(argv[2]);
   char content[len];
   if (vtp_read(fd, content, len) < 0) {
      return NULL;
   }

   vfsn_t *node = vtp_path(*cwd, argv[1]);
   if (!node) {
      return ERR_NOSUCHFILE;
   }
   
   vfs_write(node, content, len);
   vfs_close(node);
   return MSG_UPDATED;
}

static char* vtp_cmd_cd(int fd, vfsn_t **cwd, int argc, char* argv[])
{
   vfsn_t *next = vtp_path(*cwd, argv[1]);
   if (!next)
      return ERR_NOSUCHDIR;

   vfs_close(*cwd);
   *cwd = next;
   return MSG_DIRCHANGED;
}

static char* vtp_cmd_pwd(int fd, vfsn_t **cwd, int argc, char* argv[])
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
      vfs_parent2(&it);
   }
   vtp_write(fd, index+1);

   return NULL;
}

static char* vtp_cmd_exit(int fd, vfsn_t **cwd, int argc, char* argv[])
{
   close(fd);
   return NULL;
}

static struct vtp_cmd cmds[] = {
   { "ls", 0, vtp_cmd_list },
   { "list", 0, vtp_cmd_list },
   { "create", 2, vtp_cmd_create },
   { "createdir", 1, vtp_cmd_createdir },
   { "mkdir", 1, vtp_cmd_createdir },
   { "delete", 1, vtp_cmd_delete },
   { "rm", 1, vtp_cmd_delete },
   { "exit", 0, vtp_cmd_exit },
   { "read", 1, vtp_cmd_read },
   { "cat", 1, vtp_cmd_read },
   { "update", 2, vtp_cmd_update },
   { "cd", 1, vtp_cmd_cd },
   { "pwd", 0, vtp_cmd_pwd },
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

void vtp_handle(int fd, vfsn_t *cwd)
{
   char buf[READ_BUFFER_SIZE];

   // send welcome
   dprintf(fd, "%s\n%s", MSG_WELCOME, MSG_LINE_START);

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
      int argi = 0;
      char* argv[4];
      argv[0] = strtok(buf, " \r\n");
      while (argi < 4 && argv[argi]) {
         argi++;
         argv[argi] = strtok(NULL, " \r\n");
      }

      // get command from name
      struct vtp_cmd *cmd = vtp_get_cmd(argv[0], len-2);

      // check if command was found
      if (!cmd) {
         vtp_write(fd, ERR_NOSUCHCMD);
         vtp_write(fd, MSG_LINE_START);
         continue;
      }

      // check number of arguments
      if (cmd->args + 1 > argi) {
         vtp_write(fd, ERR_INVALIDCMD);
         vtp_write(fd, MSG_LINE_START);
         continue;
      }
    
      char *errmsg = cmd->func(fd, &cwd, argi, argv);
      if (errmsg) {
         vtp_write(fd, errmsg);
      }

      vtp_write(fd, MSG_LINE_START);
   }

   // cleanup
   vfs_close(cwd);
}
