#ifndef _VMM_STD_IO_H
#define _VMM_STD_IO_H

#include "vmm/std/types.h"
#include "vmm/std/fptr.h"
#include <netdb.h>

int    Open ( const char *pathname, int oflag );
int    Open_fmt ( int oflag, const char *fmt, ... );
int    Open2 ( const char *pathname, int oflag, mode_t mode );
int    Open2_fmt ( int oflag, mode_t mode, const char *fmt, ... );
int    Creat ( const char *pathname, mode_t mode );
int    Creat_fmt ( mode_t mode, const char *fmt, ... );
void   Close ( int fd );
void   Truncate ( const char *path, off_t length );
void   Ftruncate ( int fd, off_t length );
off_t  Lseek ( int fildes, off_t offset, int whence );
size_t Get_filesize ( int fd );

ssize_t Read ( int fd, void *buf, size_t count );
void    Readn ( int fd, void *buf, size_t count );
ssize_t Write ( int fd, const void *buf, size_t count );
void    Writen ( int fd, const void *buf, size_t count );
int     Fcntl ( int fd, int cmd, long arg );

FILE   *Fopen ( const char *path, const char *mode );
FILE   *Fopen_fmt ( const char *mode, const char *fmt, ... );
void   Fclose ( FILE *fp );
int    Select ( int n, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout );
bool_t Fd_is_readable ( int fd );

void   Mkfifo ( const char *pathname, mode_t mode );
void   Copy_file ( const char *from, const char *to );
void   Remove ( const char *pathname );

void Pack ( const void *p, size_t len, int fd );
void Unpack ( void *p, size_t len, int fd );
struct fptr_t Unpack_any ( int fd );

#define DEFINE_PACK_UNPACK_FUNC_PROTOTYPE(fun_name, type) \
void fun_name ## _pack ( type x, int fd ); \
type fun_name ## _unpack ( int fd ); \
void fun_name ## Array_pack ( const type x[], size_t nelem, int fd ); \
void fun_name ## Array_unpack ( type x[], size_t nelem, int fd );

DEFINE_PACK_UNPACK_FUNC_PROTOTYPE(Bool, bool_t)
DEFINE_PACK_UNPACK_FUNC_PROTOTYPE(Bit8u, bit8u_t)
DEFINE_PACK_UNPACK_FUNC_PROTOTYPE(Bit16u, bit16u_t)
DEFINE_PACK_UNPACK_FUNC_PROTOTYPE(Bit32u, bit32u_t)
DEFINE_PACK_UNPACK_FUNC_PROTOTYPE(Bit64u, bit64u_t)

#endif /* _VMM_STD_IO_H */
