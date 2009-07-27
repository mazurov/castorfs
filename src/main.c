/**
 *      @file  main.c
 *      @brief  FUSE for CASTOR
 *
 * Implementation of FUSE function for access CASTOR.
 *
 *     @author  Alexander MAZUROV (alexander.mazurov@cern.ch
 *
 *   @internal
 *     Created  01/21/2009
 *    Revision  1
 *    Compiler  gcc
 *     Company  CERN, Switzerland
 *   Copyright  Copyright (c) 2009, Alexander MAZUROV
 *
 * This source code is released for free distribution under the terms of the
 * GNU General Public License as published by the Free Software Foundation.
 * =====================================================================================
 */

/* #####   MACROS  -  LOCAL TO THIS SOURCE FILE   ################################### */
#define FUSE_USE_VERSION 26
#define _LARGEFILE64_SOURCE

#define PATH_SIZE_MAX 512

#define XATTR_NAME_SIZE_MAX 65
#define XATTR_VALUE_SIZE_MAX 65
#define XATTR_NUM_SEGMENTS_MAX 5
#define XATTR_LIST_SIZE_MAX 512

#define XATTR_STATUS "user.status"
#define XATTR_CHECKSUM_NAME "user.checksum_name"
#define XATTR_CHECKSUM "user.checksum"
#define XATTR_NBSEG "user.nbseg"

#define CASTOR_ROOT "/castor"
#define CASTORFS_OPT(t, p, v) { t, offsetof(struct castorfs, p), v }

#define DEBUG(format, args...)  \
  do { if (castorfs.debug) fprintf(stderr, format, args); } while(0)

/* #####   HEADER FILE INCLUDES   ################################################### */
#if HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <sys/time.h>
#include <stdlib.h>

#include <fuse/fuse.h> /* FUSE */
#include <Cthread_api.h> /* Castor - Threads */
#include "Cns_api.h" /* Castor - Oracle Interface */
#include "rfio_api.h" /* Castor */

/* #####   TYPE DEFINITIONS  -  LOCAL TO THIS SOURCE FILE   ######################### */

/* #####   DATA TYPES  -  LOCAL TO THIS SOURCE FILE   ############################### */
struct castorfs
{
  char *root;
  uid_t uid;
  gid_t gid;
  int readonly;
  int debug;
  char *stage_user;
  char *stage_host;
  char *stage_svcclass;
};

enum {
	KEY_HELP,
  KEY_VERSION
};

/* #####   VARIABLES  -  LOCAL TO THIS SOURCE FILE   ################################ */
static struct castorfs castorfs;
static struct fuse_opt castorfs_opts[] = {
  CASTORFS_OPT("castor_root=%s",  root, 0),
  CASTORFS_OPT("castor_stage_host=%s",  stage_host, 0),
  CASTORFS_OPT("castor_stage_svcclass=%s",  stage_svcclass, 0),
  CASTORFS_OPT("castor_user=%s",  stage_user, 0),
  CASTORFS_OPT("castor_uid=%d",   uid, 0),
  CASTORFS_OPT("castor_gid=%d",   gid, 0),
  CASTORFS_OPT("castor_readonly", readonly,1),

	FUSE_OPT_KEY("-V",          KEY_VERSION),
	FUSE_OPT_KEY("--version",   KEY_VERSION),
  FUSE_OPT_KEY("-h",          KEY_HELP),
  FUSE_OPT_KEY("--help",      KEY_HELP),

  FUSE_OPT_END
};


static char  xattrlist[XATTR_LIST_SIZE_MAX];
/** If file has less segments then maximum allowed we need to cut 
 *  attribute list to (number of segments)*xattrlist_segment_len
 */
static int   xattrlist_len = 0;
static int   xattrlist_segment_len = 0;
/** If there are no segments we need cut attribute list to 
 *  the xattrlist_segment_sum_len lenght
 */
static int   xattrlist_segment_sum_len = 0;
/* #####   PROTOTYPES  -  LOCAL TO THIS SOURCE FILE   ############################### */

/* #####   FUNCTION DEFINITIONS  -  EXPORTED FUNCTIONS   ############################ */

/* #####   FUNCTION DEFINITIONS  -  LOCAL TO THIS SOURCE FILE   ##################### */
static void usage(const char *progname)
{
	fprintf(stderr,
    "usage: %s [options] mountpoint\n"
"\n"
"general options:\n"
"    -o opt,[opt...]        mount options\n"
"    -h   --help            print help\n"
"    -V   --version         print version\n"
"\n"
"CASTORFS options:\n"
"    -o castor_readonly       readonly mount\n"
"    -o castor_user           CASTOR user name\n"
"                             (default:  getpwuid(uid))\n"
"    -o castor_uid            CASTOR user uid\n"
"    -o castor_gid            CASTOR user gid\n"
"    -o castor_root           CASTOR root directory (default: '/castor')\n"
"    -o castor_stage_host     CASTOR stage host\n"
"                             (set environment variable STAGE_HOST)\n"
"    -o castor_stage_svcclass CASTOR stage service class\n"
"                             (set environment variable STAGE_SVCCLASS)\n"
"\n", progname);
}
/**
 * @brief  Construct CASTOR absolute path
 * @param  relative_path CASTOR path relative to fuse mount point
 * @return CASTOR absolute path 
 */
static char* absolute_path(const char* relative_path,char *result)
{
   int size = strlen(castorfs.root)+strlen(relative_path)+1;
   //char* result = (char*)calloc(sizeof(char),size);
   strncpy(result,castorfs.root,PATH_SIZE_MAX);
   return strncat(result,relative_path,PATH_SIZE_MAX);
}
/* ---------------------------------------------------------------------------------- */

static int cfuse_getsegattrs(const char* relative_path,int *nbseg, 
                                                          struct Cns_segattrs **xattrs)
{
  char path[PATH_SIZE_MAX];
  absolute_path(relative_path,path);
  int res = Cns_getsegattrs(path,NULL,nbseg,xattrs);
  return res;
}
/* ---------------------------------------------------------------------------------- */

/**
 * @brief  Helper function
 *         (1) Add new attribute to attribute list. 
 *         (2) Increment total length of list. 
 *         (3) Change list pointer to the end of list
 * @param  attribute Attribute which sould be added
 * @param  list Target list (inout).
 * @param  len Total length of list (out).
 * @param maxsize Maximum size of added attribute (out)
 * @see cfuse_init_xattrlist
 */
static void _cfuse_add_attribute(const char *attribute,char** list, int *len, int *maxsize)
{
  int l = strlen(attribute);
  memcpy(*list,attribute,l+1);
  *list = *list+l+1;
  maxsize = maxsize-l+1;
  *len += l+1;
}
/* ---------------------------------------------------------------------------------- */

/**
 * @brief  Init full list of available attributes.
 *         When it will be called cfuse_listxattr the list will be reduced 
 */
static void cfuse_init_xattrlist()
{
   char *current = xattrlist;
   int max = XATTR_LIST_SIZE_MAX;
   int len = 0;
   _cfuse_add_attribute(XATTR_STATUS,&current,&xattrlist_len,&max);
   _cfuse_add_attribute(XATTR_NBSEG,&current,&xattrlist_len,&max);
   _cfuse_add_attribute(XATTR_CHECKSUM_NAME,&current,&xattrlist_len,&max);
   _cfuse_add_attribute(XATTR_CHECKSUM,&current,&xattrlist_len,&max);
   
   xattrlist_segment_sum_len = strlen(XATTR_CHECKSUM_NAME)+strlen(XATTR_CHECKSUM)+2;

   int i=1;
   for (i=1; i <= XATTR_NUM_SEGMENTS_MAX; i++) {
     char name[XATTR_NAME_SIZE_MAX];
     snprintf(name,XATTR_NAME_SIZE_MAX,"castor.seg%d.checksum_name",i);
     if ( i==1 ) xattrlist_segment_len += strlen(name)+1;
     _cfuse_add_attribute(name,&current,&xattrlist_len,&max);
     snprintf(name,XATTR_NAME_SIZE_MAX,"castor.seg%d.checksum",i);
     if ( i==1 ) xattrlist_segment_len += strlen(name)+1;
     _cfuse_add_attribute(name,&current,&xattrlist_len,&max);
   }
}
/* ---------------------------------------------------------------------------------- */
static int cfuse_cns_stat(const char *relative_path, struct Cns_filestat *stat)
{
  char path[PATH_SIZE_MAX];
  absolute_path(relative_path,path);
  int res = Cns_lstat(path,stat);
  return res;
}
/* ---------------------------------------------------------------------------------- */

static void cfuse_free_segattrs(struct Cns_segattrs **attrs,int size)
{
  int i=0;
  for (i=0;i++;i<size) {
    struct Cns_segattrs *current = *attrs+i*sizeof(struct Cns_segattrs);
    free(current);
  }
  free(*attrs);
}
/* ---------------------------------------------------------------------------------- */

int cfuse_init_account()
{
  int res=0;
  if ( 0 != castorfs.gid ) {
    res = setgid(castorfs.gid);
    if ( -1 == res){
      DEBUG("main.c: could not setup uid to %d\n",castorfs.uid);
      return -1;
    }
  }
  if ( 0 != castorfs.uid) {
    res = setuid(castorfs.uid);
    if ( -1 == res){
      DEBUG("main.c: could not setup gid to %d\n",castorfs.gid);
      return -1;
    }
  }
  return 0;
}
/* ---------------------------------------------------------------------------------- */

void cfuse_debug_account()
{
   DEBUG("uid=%d,gid=%d,euid=%d,egid=%d",getuid(),getgid(),geteuid(),getegid());
}
/* ---------------------------------------------------------------------------------- */

/** @defgroup HOOKS  FUSE hooks
 * @{
 */


/**
 * @brief  Implementation of FUSE hook "getattr"
 * @param  relative_path
 * @param stbuf
 * @return 
 */
static int cfuse_getattr(const char* relative_path, struct stat *stbuf)
{
  memset(stbuf, 0, sizeof(struct stat));
  char path[PATH_SIZE_MAX];
  absolute_path(relative_path,path);
  DEBUG("PATH=%s\n",path);
  int res = rfio_stat(path,stbuf);

  if ( -1 == res) return -rfio_serrno();

  return 0;
}

/* ---------------------------------------------------------------------------------- */

/**
 * @brief  Implementation of FUSE hook "access"
 * @param  relative_path
 * @param mask
 * @return 
 */
static int cfuse_access(const char* relative_path, int mask)
{
 /* REMAINS TO BE IMPLEMENTED */
  (void)relative_path;

  return 0;
}
/* ---------------------------------------------------------------------------------- */
static int cfuse_chown(const char *relative_path, uid_t uid, gid_t gid)
{
  if (castorfs.readonly) return -EACCES;
  
  char path[PATH_SIZE_MAX];
  absolute_path(relative_path,path);

  return rfio_chown(path,uid,gid);
}
/* ---------------------------------------------------------------------------------- */

/**
 * @brief  Implementation of FUSE hook "readdir"
 * @param relative_path
 * @param buf
 * @param filler
 * @param offset
 * @param fi
 * @return 
 */
static int cfuse_readdir(const char* relative_path, void *buf, fuse_fill_dir_t filler,
                                              off_t offset, struct fuse_file_info *fi)
{

  struct Cns_direnstat *de;
  char path[PATH_SIZE_MAX];
  absolute_path(relative_path,path);
  Cns_DIR* dp = Cns_opendir(path);
  if (dp == NULL) return -errno;
  while ((de = Cns_readdirx(dp)) != NULL) {
    struct stat st;
    memset(&st, 0, sizeof(st));
    st.st_mode = de->filemode;

    if (filler(buf, de->d_name, &st, 0))
      break;
  }

  Cns_closedir(dp);
  return 0;
}
/* ---------------------------------------------------------------------------------- */

/**
 * @brief  Implementation of FUSE hook "create"
 * @param  relative_path
 * @param  mode
 * @param  fi
 * @return 
 */
static int cfuse_create(const char* relative_path, mode_t mode, 
                                                          struct fuse_file_info *fi)
{
  if (castorfs.readonly) return -EACCES;
  char path[PATH_SIZE_MAX];
  absolute_path(relative_path,path);
  int fd = rfio_open64(path,O_WRONLY|O_CREAT|O_TRUNC /*fi->flags*/,mode);
  if (fd == -1) {
    DEBUG("cfuse_create: %s",rfio_serror());
    return -rfio_serrno();
  }

  fi->fh = fd;
  return 0;
}
/* ---------------------------------------------------------------------------------- */

/**
 * @brief  Implementation of FUSE hook "open"
 * @param relative_path
 * @param fi
 * @return 
 */
static int cfuse_open(const char* relative_path, struct fuse_file_info *fi)
{
  char path[PATH_SIZE_MAX];
  absolute_path(relative_path,path);
  int fd = rfio_open64(path,fi->flags, 0644);
  if (fd == -1) return -rfio_serrno();

  fi->fh = fd;
  return 0;
}
/* ---------------------------------------------------------------------------------- */

/**
 * @brief  Implementation of FUSE hook "read"
 * @param relative_path
 * @param buf
 * @param size
 * @param offset
 * @param fi
 * @return 
 */
static int cfuse_read(const char* relative_path, char *buf, size_t size, off_t offset,
      struct fuse_file_info *fi)
{

  int fd;
  (void)relative_path;

  rfio_lseek64(fi->fh,offset,SEEK_SET);

  int res = rfio_read(fi->fh,buf, size);
  if (res == -1) {
    DEBUG("cfuse_read: %s",rfio_serror());
    return -rfio_serrno();
  }

  return res;
}
/* ---------------------------------------------------------------------------------- */

/**
 * @brief  Implementation of FUSE hook "write"
 * @param  relative_path
 * @param  buf
 * @param size
 * @param offset
 * @param fi
 * @return 
 */
static int cfuse_write(const char* relative_path, const char *buf, size_t size,
         off_t offset, struct fuse_file_info *fi)
{
  if (castorfs.readonly) return -EACCES;

  (void)relative_path;

  rfio_lseek64(fi->fh,offset,SEEK_SET); 
  int res = rfio_write(fi->fh,(void*)buf, size);
  if (res == -1) {
    DEBUG("cfuse_write: %s",rfio_serror());
    return -rfio_serrno();
  }
  return res;
}
/* ---------------------------------------------------------------------------------- */

/**
 * @brief  Implementation of FUSE hook "release"
 * @param  relative_path
 * @param  fi
 * @return 
 */
static int cfuse_release(const char* relative_path, struct fuse_file_info *fi)
{
  (void)relative_path;
  rfio_close(fi->fh);

  return 0;
}
/* ---------------------------------------------------------------------------------- */

/**
 * @brief  Implementation of FUSE hook "unlink"
 * @param  relative_path
 * @return 
 */
static int cfuse_unlink(const char* relative_path)
{
  if (castorfs.readonly) return -EACCES;

  char path[PATH_SIZE_MAX];
  absolute_path(relative_path,path);
  int res = rfio_unlink(path);
  if (res == -1) {
    DEBUG("cfuse_unlink: %s",rfio_serror());
    return -rfio_serrno();
  }
  return res;
}
/* ---------------------------------------------------------------------------------- */

/**
 * @brief  Implementation of FUSE hook "mkdir"
 * @param  relative_path
 * @param mode
 * @return 
 */
static int cfuse_mkdir(const char* relative_path, mode_t mode)
{
  if (castorfs.readonly) return -EACCES;

  char path[PATH_SIZE_MAX];
  absolute_path(relative_path, path);
  int res = rfio_mkdir(path, mode);
  if (res == -1) res =  -rfio_serrno();

  return res;
}

/* ---------------------------------------------------------------------------------- */
/**
 * @brief  Implementation of FUSE hook "rmdir"
 * @param  relative_path
 * @return 
 */
static int cfuse_rmdir(const char* relative_path)
{
  char path[PATH_SIZE_MAX];
  absolute_path(relative_path,path);
  int res = rfio_rmdir(path);
  if (res == -1)  res = rfio_serrno();

  return res;
}
/* ---------------------------------------------------------------------------------- */

/**
 * @brief  Implementation of FUSE hook "truncate"
 * @param  relative_path
 * @param  size
 * @return 
 */
static int cfuse_truncate(const char* relative_path, off_t size)
{

  if (castorfs.readonly) return -EACCES;
  (void)size;

  // We can truncate only by recreating file
  struct fuse_file_info fi;
  int res = cfuse_create(relative_path,0644,&fi); // create
  if ( 0 > res) return -rfio_serrno();
  rfio_close(fi.fh); // close file handler
  return res;

}
/* ---------------------------------------------------------------------------------- */

/**
 * @brief  Implementation of FUSE hook "utimens"
 * @param  relative_path
 * @param  ts
 * @return 
 */
static int cfuse_utimens(const char *relative_path, const struct timespec ts[2])
{
   (void)relative_path;
   (void)ts;

    return 0;
}
/* ---------------------------------------------------------------------------------- */

/**
 * @brief  Implementation of FUSE hook "listxattr"
 * @param  relative_path
 * @param  list
 * @param  size
 * @return 
 */
static int cfuse_listxattr(const char *relative_path, char *list, size_t size)
{
  if (size==0) return XATTR_LIST_SIZE_MAX;

  struct Cns_segattrs *attrs;
  int nbseg=0;
  int res = cfuse_getsegattrs(relative_path,&nbseg,&attrs);
  if (-1 == res) return -1;

  cfuse_free_segattrs(&attrs,nbseg);
  
  // We should cut list of attributes if number of segments less then 
  int len = xattrlist_len -(XATTR_NUM_SEGMENTS_MAX-nbseg)*xattrlist_segment_len;
  // If there are no attributes we should delete summary info about segments
  if (nbseg == 0) len = len-xattrlist_segment_sum_len;

  memcpy(list,xattrlist,len);
  return len;
}
/* ---------------------------------------------------------------------------------- */

/**
 * @brief Implementation of FUSE hook "getxattr" 
 * @param  relative_path
 * @param  name
 * @param  value
 * @param  size
 * @return 
 */
static int cfuse_getxattr(const char *relative_path, const char *name, char *value,
      size_t size)
{
  if ( 0 == size) return XATTR_SIZE_MAX;
  //fprintf(stderr,"name=%s\n",name);
  strncpy(value,"",size);
  struct Cns_filestat stat;
  int res = cfuse_cns_stat(relative_path ,&stat);
  if (0 > res) return res;
  if (0 == strcmp(name,XATTR_STATUS)) {
    switch(stat.status) {
      case 'm':
           strncpy(value,"migrated",size);
           break;
      case 'o':
           strncpy(value,"online",size);
           break;
      default:
           strncpy(value,"unknown",size);
           break;
          
    }
    return strlen(value);
  }

  if ( (0 == strncmp(name,"castor.seg",strlen("castor.seg")-1)) 
    || (0 == strncmp(name,XATTR_CHECKSUM,strlen(XATTR_CHECKSUM)-1)) 
    || (0 == strcmp(name,XATTR_NBSEG))) {
    int nbseg=0;
    struct Cns_segattrs *attrs;
    res = cfuse_getsegattrs(relative_path,&nbseg,&attrs);
    if (0 != res) return -errno;
    if (0 == strcmp(name,XATTR_NBSEG)) {
      snprintf(value,size,"%d",nbseg);
    } else if (0 != nbseg) {
      int s=0;
      for (s=0; s < nbseg;s++) {
        struct Cns_segattrs* segment = attrs+s*sizeof(struct Cns_segattrs);
        char segattrname[XATTR_NAME_SIZE_MAX];
        snprintf(segattrname,XATTR_NAME_SIZE_MAX,"castor.seg%d.checksum_name",s+1);
        if ((0 == strcmp(name,segattrname))
          || (s==0 && (0 == strcmp(name,XATTR_CHECKSUM_NAME)))) {
          strncpy(value,segment->checksum_name,size);
          break;
        }else {
          snprintf(segattrname,XATTR_NAME_SIZE_MAX,"castor.seg%d.checksum",s+1);
          if ((0 == strcmp(name,segattrname)) 
            || (s==0 && (0 == strcmp(name,XATTR_CHECKSUM)))){
            snprintf(value,size,"%lu",segment->checksum);
            break;
          }
        }
      }
    }
    cfuse_free_segattrs(&attrs,nbseg);
  }
  int len = strlen(value);
  return len;
}
/* ---------------------------------------------------------------------------------- */


/**
 * @brief Implementation of FUSE hook "removexattr" 
 * @param  relative_path
 * @param  name
 * @return 
 */
static int cfuse_removexattr(const char *relative_path, const char *name)
{
  return 0;
}
/** ---------------------------------------------------------------------------------- 
 * @} HOOKS
 *  ----------------------------------------------------------------------------------
 */
static struct fuse_operations cfuse_oper = 
  {
    .getattr = cfuse_getattr,
    .readdir = cfuse_readdir,
    .create = cfuse_create,
    .open = cfuse_open,
    .read = cfuse_read,
    .write = cfuse_write,
    .release = cfuse_release,
    .unlink = cfuse_unlink,
    .mkdir = cfuse_mkdir,
    .rmdir = cfuse_rmdir,
    .truncate = cfuse_truncate,
    .utimens = cfuse_utimens,
    .getxattr = cfuse_getxattr,
    .listxattr = cfuse_listxattr,
    .removexattr = cfuse_removexattr,
    .chown = cfuse_chown
  };

static int cfuse_main(struct fuse_args *args)
{
	return fuse_main(args->argc, args->argv, &cfuse_oper, NULL);
}
/* ---------------------------------------------------------------------------------- */

static int cfuse_opt_proc(void *data, const char *arg, int key, 
                                                          struct fuse_args *outargs)
{
  switch(key) {
    case FUSE_OPT_KEY_OPT:
      if ( 0 == strcmp(arg,"-d")) castorfs.debug =1;
      break;
    case KEY_HELP:
		  usage(outargs->argv[0]);
		  fuse_opt_add_arg(outargs, "-ho");
		  cfuse_main(outargs);
		  exit(1);
    case KEY_VERSION:
		  fprintf(stderr, "castorfs version %s\n", CASTORFS_VERSION);
		  fuse_opt_add_arg(outargs, "--version");
		  cfuse_main(outargs);
		  exit(0);
    default:
      break;
  }
  return 1;
}
/* ---------------------------------------------------------------------------------- */

int main(int argc, char** argv)
{
  struct fuse_args args = FUSE_ARGS_INIT(argc, argv); 
  char *castor_default_root = "/castor";
  castorfs.root           = castor_default_root;
  castorfs.uid            = 0;
  castorfs.gid            = 0;
  castorfs.readonly       = 0;
  castorfs.stage_user     = NULL;
  castorfs.stage_host     = NULL;
  castorfs.stage_svcclass = NULL;

  int res = fuse_opt_parse(&args, &castorfs, castorfs_opts, cfuse_opt_proc);
  // Without this readinf will not work
  fuse_opt_add_arg(&args,"-osync_read");

  // Set environment variables
  setenv("RFIO_USE_CASTOR_V2","YES",1); // We use only new version of CASTOR
  if ( NULL != castorfs.stage_user ) {
    setenv("STAGE_USER",castorfs.stage_user,1);
  }
  if ( NULL != castorfs.stage_host ) {
    setenv("STAGE_HOST",castorfs.stage_host,1);
  }
  if ( NULL != castorfs.stage_host ) {
    setenv("STAGE_HOST",castorfs.stage_host,1);
  }
  if ( NULL != castorfs.stage_svcclass ) {
    setenv("STAGE_SVCCLASS",castorfs.stage_svcclass,1);
  }

  /*int i=0;
  for  (i=0; i<args.argc; i++)
  {
    fprintf(stderr,"%d:%s ",i,args.argv[i]);
  }*/
  cfuse_init_xattrlist();
  Cthread_init();
  cfuse_init_account();
  //cfuse_debug_account();

  res = cfuse_main(&args);
  fuse_opt_free_args(&args);
  return res;
}
/* --------------------------------------------------------------------------*/

