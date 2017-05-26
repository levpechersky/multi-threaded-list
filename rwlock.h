/*
 * rwlock.h
 *
 *  Created on: 26 May 2017
 *      Author: Lev
 */

#ifndef RWLOCK_H_
#define RWLOCK_H_

typedef struct rwlock {
	int number_of_readers;
	pthread_cond_t readers_condition;
	int number_of_writers;
	pthread_cond_t writers_condition;
	pthread_mutex_t global_lock;
} rwlock_t;

void rw_lock_init(rwlock_t* lock);
void rw_lock_destroy(rwlock_t* lock);
void read_lock(rwlock_t* lock);
void read_unlock(rwlock_t* lock);
void write_lock(rwlock_t* lock);
void write_unlock(rwlock_t* lock);

#endif /* RWLOCK_H_ */
