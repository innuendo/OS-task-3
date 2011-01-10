#ifndef _SIMPLE_SEM_H_
#define _SIMPLE_SEM_H_

int  sem_initialize (key_t key, int flags);
void sem_done (int sem_id);
void P (int sem_id);
void V (int sem_id);

#endif
