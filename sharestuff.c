#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <errno.h>
#include "sharestuff.h"

#ifndef errno
extern int errno;
#endif

#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

/* Use Perl's memory management */

#ifndef Newxz
#define Newxz(pointer, number, type) \
    Newz(1, pointer, number, type)
#endif

#ifdef HAS_UNION_SEMUN
#define SEMUN union semun
#else
union my_semun {
  int val;
  struct semid_ds *buf;
  unsigned short *array;
};
#define SEMUN union my_semun
#endif

/* --- DEFINE MACROS FOR SEMAPHORE OPERATIONS --- */

#define GET_EX_LOCK(A)    semop((A), &ex_lock[0],    3)
#define GET_EX_LOCK_NB(A) semop((A), &ex_lock_nb[0], 3)
#define RM_EX_LOCK(A)     semop((A), &ex_unlock[0],  1)
#define GET_SH_LOCK(A)    semop((A), &sh_lock[0],    2)
#define GET_SH_LOCK_NB(A) semop((A), &sh_lock_nb[0], 2)
#define RM_SH_LOCK(A)     semop((A), &sh_unlock[0],  1)

/* --- DEFINE STRUCTURES FOR MANIPULATING SEMAPHORES --- */

static struct sembuf ex_lock[3] = {
  {1, 0, 0},                    /* wait for readers to finish */
  {2, 0, 0},                    /* wait for writers to finish */
  {2, 1, SEM_UNDO}              /* assert write lock */
};

static struct sembuf ex_lock_nb[3] = {
  {1, 0, IPC_NOWAIT},           /* wait for readers to finish */
  {2, 0, IPC_NOWAIT},           /* wait for writers to finish */
  {2, 1, ( SEM_UNDO | IPC_NOWAIT )}     /* assert write lock */
};

static struct sembuf ex_unlock[1] = {
  {2, -1, ( SEM_UNDO | IPC_NOWAIT )}    /* remove write lock */
};

static struct sembuf sh_lock[2] = {
  {2, 0, 0},                    /* wait for writers to finish */
  {1, 1, SEM_UNDO}              /* assert shared read lock */
};

static struct sembuf sh_lock_nb[2] = {
  {2, 0, IPC_NOWAIT},           /* wait for writers to finish */
  {1, 1, ( SEM_UNDO | IPC_NOWAIT )}     /* assert shared read lock */
};

static struct sembuf sh_unlock[1] = {
  {1, -1, ( SEM_UNDO | IPC_NOWAIT )}    /* remove shared read lock */
};

FILE *log_fh = NULL;
#define LOG_ARGS const char *file, int line, const char *fmt, ...
#define LOG0(fmt) sharelite_log(__FILE__, __LINE__, fmt)
#define LOG1(fmt, a1) sharelite_log(__FILE__, __LINE__, fmt, a1)
#define LOG2(fmt, a1, a2) sharelite_log(__FILE__, __LINE__, fmt, a1, a2)
#define LOG3(fmt, a1, a2, a3) sharelite_log(__FILE__, __LINE__, fmt, a1, a2, a3)

static void sharelite_log_active( LOG_ARGS );
static void sharelite_log_nop( LOG_ARGS );

static void ( *sharelite_log ) ( LOG_ARGS ) = sharelite_log_active;

static void
sharelite_log_nop( LOG_ARGS ) {
}

static void
sharelite_log_active( LOG_ARGS ) {
  if ( NULL == log_fh ) {
    const char *log_file = getenv( "IPC_SHARELITE_LOG" );
    if ( NULL == log_file
         || ( log_fh = fopen( log_file, "a" ), NULL == log_fh ) ) {
      sharelite_log = sharelite_log_nop;
      return;
    }
  }
  {
    struct timeval now;
    char timebuf[40];
    va_list ap;

    gettimeofday( &now, NULL );
    strftime( timebuf, sizeof( timebuf ), "%Y/%m/%d %H:%M:%S",
              gmtime( &now.tv_sec ) );
    fprintf( log_fh, "%s.%06lu %s, %d : ", timebuf,
             ( unsigned long ) now.tv_usec, file, line );
    va_start( ap, fmt );
    vfprintf( log_fh, fmt, ap );
    va_end( ap );
    fprintf( log_fh, "\n" );
    fflush( log_fh );
  }
}

/* USER INITIATED LOCK */

/* returns 0  on success -- requested operation performed   *
 * returns -1 on error                                      *
 * returns 1 if LOCK_NB specified and operation would block */
int
sharelite_lock( Share * share, int flags ) {

  /* try to obtain exclusive lock by default */
  if ( !flags ) {
    flags = LOCK_EX;
  }

  /* Check for invalid combination of flags.  Invalid combinations *
   * are attempts to obtain *both* an exclusive and shared lock or *
   * to both obtain and release a lock at the same time            */
  if ( ( ( flags & LOCK_EX ) && ( flags & LOCK_SH ) ) ||
       ( ( flags & LOCK_UN )
         && ( ( flags & LOCK_EX ) || ( flags & LOCK_SH ) ) ) ) {
    return -1;
  }

  if ( flags & LOCK_EX ) {
                         /*** WANTS EXCLUSIVE LOCK ***/
    /* If they already have an exclusive lock, just return */
    if ( share->lock & LOCK_EX ) {
      return 0;
    }
    /* If they currently have a shared lock, remove it */
    if ( share->lock & LOCK_SH ) {
      if ( RM_SH_LOCK( share->semid ) < 0 ) {
        return -1;
      }
      share->lock = 0;
    }
    if ( flags & LOCK_NB ) {    /* non-blocking request */
      if ( GET_EX_LOCK_NB( share->semid ) < 0 ) {
        if ( errno == EAGAIN ) {        /* would we have blocked? */
          return 1;
        }
        return -1;
      }
    }
    else {                      /* blocking request */
      if ( GET_EX_LOCK( share->semid ) < 0 ) {
        return -1;
      }
    }
    share->lock = LOCK_EX;
    return 0;
  }
  else if ( flags & LOCK_SH ) {
                                /*** WANTS SHARED LOCK ***/
    /* If they already have a shared lock, just return */
    if ( share->lock & LOCK_SH ) {
      return 0;
    }
    /* If they currently have an exclusive lock, remove it */
    if ( share->lock & LOCK_EX ) {
      if ( RM_EX_LOCK( share->semid ) < 0 ) {
        return -1;
      }
      share->lock = 0;
    }
    if ( flags & LOCK_NB ) {    /* non-blocking request */
      if ( GET_SH_LOCK_NB( share->semid ) < 0 ) {
        if ( errno == EAGAIN ) {        /* would we have blocked? */
          return 1;
        }
        return -1;
      }
    }
    else {                      /* blocking request */
      if ( GET_SH_LOCK( share->semid ) < 0 ) {
        return -1;
      }
    }
    share->lock = LOCK_SH;
    return 0;
  }
  else if ( flags & LOCK_UN ) {
        /*** WANTS TO RELEASE LOCK ***/
    if ( share->lock & LOCK_EX ) {
      if ( RM_EX_LOCK( share->semid ) < 0 ) {
        return -1;
      }
    }
    else if ( share->lock & LOCK_SH ) {
      if ( RM_SH_LOCK( share->semid ) < 0 ) {
        return -1;
      }
    }
  }

  return 0;
}

int
sharelite_unlock( Share * share ) {
  if ( share->lock & LOCK_EX ) {
    if ( RM_EX_LOCK( share->semid ) < 0 ) {
      return -1;
    }
  }
  else if ( share->lock & LOCK_SH ) {
    if ( RM_SH_LOCK( share->semid ) < 0 ) {
      return -1;
    }
  }
  share->lock = 0;
  return 0;
}

Node *
_add_segment( Share * share ) {
  Node *node;
  int flags;

  Newxz( node, 1, Node );

  node->next = NULL;

  /* Does another shared memory segment already exist? */
  if ( share->tail->shmaddr->next_shmid >= 0 ) {
    node->shmid = share->tail->shmaddr->next_shmid;
    if ( ( node->shmaddr =
           ( Header * ) shmat( node->shmid, ( char * ) 0,
                               0 ) ) == ( Header * ) - 1 ) {
      return NULL;
    }
    share->tail->next = node;
    share->tail = node;
    return node;
  }

  flags = share->flags | IPC_CREAT | IPC_EXCL;

  /* We need to create a new segment */
  while ( 1 ) {
    node->shmid = shmget( share->next_key++, share->segment_size, flags );
    if ( node->shmid >= 0 ) {
      break;
    }
#ifdef EIDRM
    if ( errno == EEXIST || errno == EIDRM ) {
      continue;
    }
#else
    if ( errno == EEXIST ) {
      continue;
    }
#endif
    return NULL;
  }

  share->tail->shmaddr->next_shmid = node->shmid;
  share->tail->next = node;
  share->tail = node;
  if ( ( node->shmaddr =
         ( Header * ) shmat( node->shmid, ( char * ) 0,
                             0 ) ) == ( Header * ) - 1 ) {
    return NULL;
  }
  node->shmaddr->next_shmid = -1;
  node->shmaddr->length = 0;

  return node;
}

int
_detach_segments( Node * node ) {
  Node *next_node;

  while ( node != NULL ) {
    next_node = node->next;
    if ( shmdt( ( char * ) node->shmaddr ) < 0 ) {
      return -1;
    }
    Safefree( node );
    node = next_node;
  }
  return 0;
}

int
_remove_segments( int shmid ) {
  int next_shmid;
  Header *shmaddr;

  while ( shmid >= 0 ) {
    if ( ( shmaddr =
           ( Header * ) shmat( shmid, ( char * ) 0,
                               0 ) ) == ( Header * ) - 1 ) {
      return -1;
    }
    next_shmid = shmaddr->next_shmid;
    if ( shmdt( ( char * ) shmaddr ) < 0 ) {
      return -1;
    }
    if ( shmctl( shmid, IPC_RMID, ( struct shmid_ds * ) 0 ) < 0 ) {
      return -1;
    }
    shmid = next_shmid;
  }

  return 0;
}

int
_invalidate_segments( Share * share ) {

  if ( _detach_segments( share->head->next ) < 0 ) {
    return -1;
  }
  share->head->next = NULL;
  share->tail = share->head;
  share->shm_state = share->head->shmaddr->shm_state;

  return 0;
}

int
write_share( Share * share, char *data, int length ) {
  char *shmaddr;
  int segments;
  int left;
  int chunk_size;
  Node *node;
  int shmid;

  if ( data == NULL ) {
    return -1;
  }

  if ( !( share->lock & LOCK_EX ) ) {
    if ( share->lock & LOCK_SH ) {
      if ( RM_SH_LOCK( share->semid ) < 0 ) {
        return -1;
      }
    }
    if ( GET_EX_LOCK( share->semid ) < 0 ) {
      return -1;
    }
  }

  if ( share->shm_state != share->head->shmaddr->shm_state ) {
    if ( _invalidate_segments( share ) < 0 ) {
      return -1;
    }
  }

  /* set the data length to zero.  if we are interrupted or encounter *
   * an error during the write, this guarantees that we won't         *
   * receive corrupt data in future reads.                            */
  share->head->shmaddr->length = 0;

  /* compute number of segments necessary to hold data */
  segments =
      ( length / share->data_size ) +
      ( length % share->data_size ? 1 : 0 );

  node = share->head;
  left = length;
  while ( segments-- ) {
    if ( node == NULL ) {
      if ( ( node = _add_segment( share ) ) == NULL ) {
        return -1;
      }
    }
    chunk_size = ( left > share->data_size ? share->data_size : left );
    shmaddr = ( char * ) node->shmaddr + sizeof( Header );
    memcpy( shmaddr, data, chunk_size );
    left -= chunk_size;
    data += chunk_size;
    if ( segments ) {
      node = node->next;
    }
  }

  /* set new length in header of first segment */
  share->head->shmaddr->length = length;

  /* garbage collection -- remove unused segments */
  if ( node->shmaddr->next_shmid >= 0 ) {
    shmid = node->shmaddr->next_shmid;
    if ( _detach_segments( node->next ) < 0 ) {
      return -1;
    }
    if ( _remove_segments( shmid ) < 0 ) {
      return -1;
    }
    node->shmaddr->next_shmid = -1;
    node->next = NULL;
    share->tail = node;
    share->head->shmaddr->shm_state++;
  }

  ++share->head->shmaddr->version;

  if ( !( share->lock & LOCK_EX ) ) {
    if ( RM_EX_LOCK( share->semid ) < 0 ) {
      return -1;
    }
    if ( share->lock & LOCK_SH ) {
      if ( GET_SH_LOCK( share->semid ) < 0 ) {
        return -1;
      }
    }
  }

  return 0;
}

int
read_share( Share * share, char **data ) {
  char *shmaddr;
  char *pos;
  Node *node;
  int length;
  int left;
  int chunk_size;

  if ( !share->lock ) {
    if ( GET_SH_LOCK( share->semid ) < 0 ) {
      return -1;
    }
  }

  if ( share->shm_state != share->head->shmaddr->shm_state ) {
    if ( _invalidate_segments( share ) < 0 ) {
      return -1;
    }
  }

  node = share->head;
  left = length = node->shmaddr->length;

  /* Allocate extra byte for a null at the end */
  Newxz( *data, length + 1, char );
  pos = *data;

  pos[length] = '\0';

  while ( left ) {
    if ( node == NULL ) {
      if ( ( node = _add_segment( share ) ) == NULL ) {
        goto fail;
      }
    }
    chunk_size = ( left > share->data_size ? share->data_size : left );
    shmaddr = ( char * ) node->shmaddr + sizeof( Header );
    memcpy( pos, shmaddr, chunk_size );
    pos += chunk_size;
    left -= chunk_size;
    node = node->next;
  }

  if ( !share->lock ) {
    if ( RM_SH_LOCK( share->semid ) < 0 ) {
      goto fail;
    }
  }

  return length;

fail:
  Safefree( *data );
  return -1;
}

Share *
new_share( key_t key, int segment_size, int flags ) {
  Share *share;
  Node *node;
  int semid;
  struct shmid_ds shmctl_arg;
  SEMUN semun_arg;

again:
  if ( ( semid = semget( key, 3, flags ) ) < 0 ) {
    LOG1( "semget failed (%d)", errno );
    return NULL;
  }

  /* It's possible for another process to obtain the semaphore, lock it, *
   * and remove it from the system before we have a chance to lock it.   *
   * In this case (EINVAL) we just try to create it again.               */
  if ( GET_EX_LOCK( semid ) < 0 ) {
    if ( errno == EINVAL ) {
      goto again;
    }
    LOG1( "GET_EX_LOCK failed (%d)", errno );
    return NULL;
  }

  /* XXX IS THIS THE RIGHT THING TO DO? */
  if ( segment_size <= sizeof( Header ) ) {
    segment_size = SHM_SEGMENT_SIZE;
  }

  Newxz( node, 1, Node );

  if ( ( node->shmid = shmget( key, segment_size, flags ) ) < 0 ) {
    LOG1( "shmget failed (%d)", errno );
    return NULL;
  }

  if ( ( node->shmaddr =
         ( Header * ) shmat( node->shmid, ( char * ) 0,
                             0 ) ) == ( Header * ) - 1 ) {
    LOG1( "shmat failed (%d)", errno );
    return NULL;
  }

  node->next = NULL;

  Newxz( share, 1, Share );

  share->key = key;
  share->next_key = key + 1;
  share->flags = flags;
  share->semid = semid;
  share->lock = 0;
  share->head = node;
  share->tail = node;

  /* is this a newly created segment?  if so, initialize it */
  if ( ( semun_arg.val =
         semctl( share->semid, 0, GETVAL, semun_arg ) ) < 0 ) {
    LOG1( "shmctl failed (%d)", errno );
    return NULL;
  }

  if ( semun_arg.val == 0 ) {
    semun_arg.val = 1;
    if ( semctl( share->semid, 0, SETVAL, semun_arg ) < 0 ) {
      LOG1( "shmctl failed (%d)", errno );
      return NULL;
    }
    share->head->shmaddr->length = 0;
    share->head->shmaddr->next_shmid = -1;
    share->head->shmaddr->shm_state = 1;
    share->head->shmaddr->version = 1;
  }

  share->shm_state = share->head->shmaddr->shm_state;
  share->version = share->head->shmaddr->version;

  /* determine the true length of the segment.  this may disagree *
   * with what the user requested, since shmget() calls will      *
   * succeed if the requested size <= the existing size           */
  if ( shmctl( share->head->shmid, IPC_STAT, &shmctl_arg ) < 0 ) {
    LOG1( "shmctl failed (%d)", errno );
    return NULL;
  }

  share->segment_size = shmctl_arg.shm_segsz;
  share->data_size = share->segment_size - sizeof( Header );

  if ( RM_EX_LOCK( semid ) < 0 ) {
    LOG1( "RM_EX_LOCK failed (%d)", errno );
    return NULL;
  }

  return share;
}

unsigned int
sharelite_version( Share * share ) {
  return share->head->shmaddr->version;
}

int
destroy_share( Share * share, int rmid ) {
  int semid;
  SEMUN semctl_arg;

  if ( !( share->lock & LOCK_EX ) ) {
    if ( share->lock & LOCK_SH ) {
      if ( RM_SH_LOCK( share->semid ) < 0 ) {
        return -1;
      }
    }
    if ( GET_EX_LOCK( share->semid ) < 0 ) {
      return -1;
    }
  }

  semid = share->head->shmid;
  if ( _detach_segments( share->head ) < 0 ) {
    return -1;
  }

  if ( rmid ) {
    if ( _remove_segments( semid ) < 0 ) {
      return -1;
    }
    semctl_arg.val = 0;
    if ( semctl( share->semid, 0, IPC_RMID, semctl_arg ) < 0 ) {
      return -1;
    }
  }
  else {
    if ( RM_EX_LOCK( share->semid ) < 0 ) {
      return -1;
    }
  }

  Safefree( share );

  return 0;
}

int
sharelite_num_segments( Share * share ) {
  int count = 0;
  int shmid;
  Header *shmaddr;

  shmid = share->head->shmid;
  while ( shmid >= 0 ) {
    count++;
    if ( ( shmaddr =
           ( Header * ) shmat( shmid, ( char * ) 0,
                               0 ) ) == ( Header * ) - 1 ) {
      return -1;
    }
    shmid = shmaddr->next_shmid;
    if ( shmdt( ( char * ) shmaddr ) < 0 ) {
      return -1;
    }
  }

  return count;
}

void
_dump_list( Share * share ) {
  Node *node;

  node = share->head;
  while ( node != NULL ) {
    printf( "shmid: %i\n", node->shmid );
    node = node->next;
  }
}
