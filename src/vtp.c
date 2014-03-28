#include "vtp.h"
#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <fcntl.h>

#define MSG_WELCOME "hello client and welcome to multithreading fileserver\n"
#define MSG_GOODBYE "Good bye!\n"
#define MSG_LINE_START "> "
#define MSG_FILECREATED "FILECREATED\n"
#define ERR_NOSUCHFILE "NOSUCHFILE No such file\n"
#define ERR_NOSUCHCMD "NOSUCHCMD No such command\n"
#define ERR_INVALIDCMD "INVALIDCMD Invalid arguments\n"
#define ERR_FILEEXISTS "FILEEXISTS File already exists\n"

struct vtp_cmd {
   char* name;
   char* (*func)(int fd, vfsn_t *root, vfsn_t *cwd, int argc, char* argv[]);
};

static vfsn_t* vtp_path(vfsn_t *cwd, char* path)
{
   vfsn_t *it = vfs_child(cwd);
   while (it) {
      char name[256];
      vfs_name(it, name, 256);
      if (strcmp(name, path) == 0) { 
         return it;
      }
      vfsn_t* tmp = it;
      it = vfs_next(it);
      vfs_close(tmp);
   }
   return NULL;
}

static int vtp_read(int fd, vfsn_t *root, char *buf, size_t size)
{
   int current = 0;
   while (current < size) {
      if (vfs_is_deleted(root))
         return 1;

      int len = read(fd, buf + current, size - current);
      if (len > 0)
         current += len;
   }
   return 0;
}

static char* vtp_cmd_create(int fd, vfsn_t *root, vfsn_t *cwd, int argc, char* argv[])
{
   if (argc < 3) {
      return ERR_INVALIDCMD;
   }

   int len = atoi(argv[2]) + 1;
   char content[len];
   memset(content, 0, len);
   if (vtp_read(fd, root, content, len)) {
      return NULL;
   }

   vfsn_t *node = vfs_create(cwd, argv[1], VFS_FILE);
   if (node) {
      vfs_write(node, content, len);
      write(fd, MSG_FILECREATED, strlen(MSG_FILECREATED));
   } else {
      return ERR_FILEEXISTS;
   }

   vfs_close(node);
   return NULL;
}

static char* vtp_cmd_delete(int fd, vfsn_t *root, vfsn_t *cwd, int argc, char* argv[])
{
   if (argc < 2) {
      return ERR_INVALIDCMD;
   }

   vfsn_t *file = vtp_path(cwd, argv[1]);
   if (!file) {
      return ERR_NOSUCHFILE;
   }

   vfs_delete(file);
   vfs_close(file);
   return NULL;
}

static char* vtp_cmd_list(int fd, vfsn_t *root, vfsn_t *cwd, int argc, char* argv[])
{
   vfsn_t *it = vfs_child(cwd);
   while (it) {
      char name[256];
      vfs_name(it, name, 256);
      write(fd, name, strlen(name));
      write(fd, "\n", 1);
      vfsn_t* tmp = it;
      it = vfs_next(it);
      vfs_close(tmp);
   }
   return NULL;
}

static char* vtp_cmd_read(int fd, vfsn_t *root, vfsn_t *cwd, int argc, char* argv[])
{
   if (argc < 2) {
      return ERR_INVALIDCMD;
   }

   vfsn_t *file = vtp_path(cwd, argv[1]);
   if (!file) {
      return ERR_NOSUCHFILE;
   }

   int size = vfs_size(file);
   char content[size];
   vfs_read(file, content, size);
   write(fd, content, size);
   write(fd, "\n", 1);
   vfs_close(file);
   return NULL;
}

static char* vtp_cmd_update(int fd, vfsn_t *root, vfsn_t *cwd, int argc, char* argv[])
{
   if (argc < 3) {
      return ERR_INVALIDCMD;
   }

   int len = atoi(argv[2]);
   char content[len];
   if (vtp_read(fd, root, content, len)) {
      return NULL;
   }

   vfsn_t *node = vtp_path(cwd, argv[1]);
   if (node) {
      vfs_write(node, content, len);
      write(fd, MSG_FILECREATED, strlen(MSG_FILECREATED));
   } else {
      write(fd, ERR_FILEEXISTS, strlen(ERR_FILEEXISTS));
   }

   vfs_close(node);
   return NULL;
}

static char* vtp_cmd_exit(int fd, vfsn_t *root, vfsn_t *cwd, int argc, char* argv[])
{
   close(fd);
   return NULL;
}

static struct vtp_cmd cmds[] = {
   { "ls", vtp_cmd_list },
   { "list", vtp_cmd_list },
   { "create", vtp_cmd_create },
   { "delete", vtp_cmd_delete },
   { "rm", vtp_cmd_delete },
   { "exit", vtp_cmd_exit },
   { "read", vtp_cmd_read },
   { "cat", vtp_cmd_read },
   { "update", vtp_cmd_update },
   { }
};

static struct vtp_cmd* vtp_get_cmd(char *name)
{
   for (struct vtp_cmd *cmd = &cmds[0]; cmd->name; cmd++) {
      if (strcmp(cmd->name, name) == 0)
         return cmd;
   }
   return NULL;
}

void vtp_handle(int fd, vfsn_t *root)
{
   vfsn_t *cwd = vfs_open(root);

   char buf[512];
   memset(buf, 0, 512);

   // send welcome
   write(fd, MSG_WELCOME, strlen(MSG_WELCOME));
   write(fd, MSG_LINE_START, strlen(MSG_LINE_START));

   while (!vfs_is_deleted(root) && fcntl(fd, F_GETFL) != -1) {
      int len;
      if ((len = read(fd, buf,512)) <= 0) {
         continue;
      }
      buf[len-2] = '\0';
      int argi = 0;
      char* argv[4];
      argv[0] = strtok(buf, " ");
      while (argi < 4 && argv[argi]) {
         argi++;
         argv[argi] = strtok(NULL, " ");
      }

      struct vtp_cmd *cmd = vtp_get_cmd(argv[0]);
      if (!cmd) {
         write(fd, ERR_NOSUCHCMD, strlen(ERR_NOSUCHCMD));
         write(fd, MSG_LINE_START, strlen(MSG_LINE_START));
         continue;
      }
    
      char *errmsg = cmd->func(fd, root, cwd, argi, argv);
      if (errmsg) {
         write(fd, errmsg, strlen(errmsg));
      }

      write(fd, MSG_LINE_START, strlen(MSG_LINE_START));
   }

   write(fd, MSG_GOODBYE, strlen(MSG_GOODBYE));
   vfs_close(cwd);
   vfs_close(root);
}
