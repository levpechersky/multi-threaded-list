/*
 * rwlock.c
 *
 *  Created on: 26 May 2017
 *      Author: Whoever wrote it for recitation 7 @ 234123
 *
 */
#include <assert.h>
#include <pthread.h>
#include "rwlock.h"


void rw_lock_init(rwlock_t* lock) {
	assert(lock);
	lock->number_of_readers = 0;
	pthread_cond_init(&lock->readers_condition, NULL);
	lock->number_of_writers = 0;
	lock->writer_waiting = 0;
	pthread_cond_init(&lock->writers_condition, NULL);
	pthread_mutex_init(&lock->global_lock, NULL);
}

void rw_lock_destroy(rwlock_t* lock) {
	assert(lock);
	pthread_cond_destroy(&lock->readers_condition);
	pthread_cond_destroy(&lock->writers_condition);
	pthread_mutex_destroy(&lock->global_lock);
}


int read_lock(rwlock_t* lock) {
	assert(lock);
	pthread_mutex_lock(&lock->global_lock);
	while (lock->writer_waiting > 0 || lock->number_of_writers > 0){
		pthread_mutex_unlock(&lock->global_lock);
		return 0;
	}
	lock->number_of_readers++;
	pthread_mutex_unlock(&lock->global_lock);
	return 1;
}

void read_unlock(rwlock_t* lock) {
	assert(lock);
	pthread_mutex_lock(&lock->global_lock);
	lock->number_of_readers--;
	if (lock->number_of_readers == 0)
		pthread_cond_signal(&lock->writers_condition);
	pthread_mutex_unlock(&lock->global_lock);
}

int write_lock(rwlock_t* lock) {
	assert(lock);
	pthread_mutex_lock(&lock->global_lock);
	while (lock->writer_waiting > 0 || lock->number_of_writers > 0){
		pthread_mutex_unlock(&lock->global_lock);
		return 0;
	}
	lock->writer_waiting = 1;
	while (lock->number_of_readers > 0)
		pthread_cond_wait(&lock->writers_condition, &lock->global_lock);
	lock->number_of_writers++;
	pthread_mutex_unlock(&lock->global_lock);
	return 1;
}

void write_unlock(rwlock_t* lock) {
	assert(lock);
	pthread_mutex_lock(&lock->global_lock);
	lock->number_of_writers--;
	if (lock->number_of_writers == 0) {
		pthread_cond_broadcast(&lock->readers_condition);
		pthread_cond_signal(&lock->writers_condition);
	}
	pthread_mutex_unlock(&lock->global_lock);
}
