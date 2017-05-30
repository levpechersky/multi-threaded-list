
/*
 * my_list.c
 *
 *  Created on: 25 May 2017
 *      Author: Lev
 */
#include <stdlib.h>
#include <limits.h>
#include <signal.h>
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <sys/types.h>


#include "my_list.h"
/* */

typedef struct rwlock {
	int number_of_readers;
	pthread_cond_t readers_condition;
	int number_of_writers;
	int writer_waiting;
	pthread_cond_t writers_condition;
	pthread_mutex_t global_lock;
} rwlock_t;

void rw_lock_init(rwlock_t* lock);
void rw_lock_destroy(rwlock_t* lock);
int read_lock(rwlock_t* lock);
void read_unlock(rwlock_t* lock);
int write_lock(rwlock_t* lock);
void write_unlock(rwlock_t* lock);


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
	if (lock->writer_waiting > 0 || lock->number_of_writers > 0){
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
	if (lock->writer_waiting > 0 || lock->number_of_writers > 0){
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
	pthread_cond_broadcast(&lock->readers_condition);
	pthread_cond_signal(&lock->writers_condition);
	pthread_mutex_unlock(&lock->global_lock);
}

/*------------------------- Types and definitions-----------------------------*/

typedef struct node_t {
	int key;
	void* data;
	struct node_t* next;
	pthread_mutex_t lock;
} node;

struct linked_list_t {
	node* head;
	int size;
	rwlock_t lock;
	pthread_mutex_t size_lock, head_lock;
};

typedef struct list_params_t {
	linked_list_t* list;
	op_t* op_param;
} list_params_t;

enum list_error {
	SUCCESS = 0,
	NULL_ARG = 2,
	INVALID_ARG,
	MEM_ERROR,
	NOT_FOUND,
	ALREADY_IN_LIST,
	DESTROY_PENDING
};

#define MALLOC_N_ORELSE(identifier, N, command) do {\
	identifier = malloc(sizeof(*(identifier))*(N)); \
	if(!(identifier)) { \
	command; \
	} } while(0)

#define MALLOC_ORELSE(identifier, command) \
		MALLOC_N_ORELSE(identifier, 1, command)

/*------------------------- Static helper functions --------------------------*/

static inline void init_node(node* new_node, int key, void* data) {
	assert(new_node);
	new_node->key = key;
	new_node->data = data;
	new_node->next = NULL;
	pthread_mutex_init(&new_node->lock, NULL);
}

/* Requires lock on node to_destroy */
static inline void destroy_node(node* to_destroy){
	assert(to_destroy);
	int r = pthread_mutex_destroy(&to_destroy->lock);
	assert(r==0);//TODO remove assert
	free(to_destroy);
}

//required locks: head
static inline void insert_first(linked_list_t* list, node* new_node) {
	assert(list && new_node);
	new_node->next = list->head;
	list->head = new_node;
}

static inline void insert_after(node* previous, node* new_node) {
	assert(previous && new_node);
	new_node->next = previous->next;
	previous->next = new_node;
}

//required locks: head, 1st node
static inline void remove_first(linked_list_t* list) {
	assert(list && list->head);
	node* to_remove = list->head;
	list->head = to_remove->next;

	pthread_mutex_unlock(&to_remove->lock);
	destroy_node(to_remove);
}

//required locks: previous, previous->next
static inline void remove_after(node* previous) {
	assert(previous && previous->next);
	node* to_remove = previous->next;
	previous->next = to_remove->next;

	pthread_mutex_unlock(&to_remove->lock);
	destroy_node(to_remove);
}

/* Return pointer to node v, where v.key < key. If for each node
 * node.key >= key (i.e. node with key should be 1st), returns NULL
 * (including the case when list is empty)
 *
 * locks:
 * if returns last node - last node locked
 * if returns NULL (only head is below key) - locks head and (if exists) the 1st node
 * otherwise locks closest below and next to it
 *
 * */
static inline node* closest_below_key(linked_list_t* list, int key) {
	assert(list);
	pthread_mutex_t *prev_lock=&list->head_lock, *next_lock=NULL;

	pthread_mutex_lock(&list->head_lock);

	node *prev = NULL, *current = list->head;
	if(list->head){
		pthread_mutex_lock(&list->head->lock);
		next_lock = &list->head->lock;
	}
	while (current && current->key < key) { //we enter here only if there's at least 1 node
		pthread_mutex_unlock(prev_lock);
		prev = current;
		prev_lock = next_lock;
		current = current->next;
		if(current){
			pthread_mutex_lock(&current->lock); //updated current, i.e. next node
			next_lock = &current->lock;
		}
	}
	return prev;
}

/* Returns with lock on found node only */
static node* find(linked_list_t* list, int key) {
	assert(list);
	node* res = NULL;
	node* prev = closest_below_key(list, key); //returns with locks
	if (prev && prev->next) { //both locked now
		if(prev->next->key == key){
			res =  prev->next;
			pthread_mutex_unlock(&prev->lock);
		} else {
			node* tmp = prev->next;
			pthread_mutex_unlock(&prev->lock);
			pthread_mutex_unlock(&tmp->lock);
		}
	} else if (prev && !prev->next) {
		// last node returned from closest_below_key
		pthread_mutex_unlock(&prev->lock);
	} 	else if (list->head) {
		// prev == NULL, but there's at least 1 node in list, and then 1st node is locked too
		if(list->head->key == key){
			res = list->head;
			pthread_mutex_unlock(&list->head_lock);
		} else {
			node* tmp = list->head;
			pthread_mutex_unlock(&list->head_lock);
			pthread_mutex_unlock(&tmp->lock);
		}
	} else { // list empty
		pthread_mutex_unlock(&list->head_lock);
	}
	return res;
}

static inline void list_init(linked_list_t* list) {
	assert(list);
	list->head = NULL;
	list->size = 0;
	pthread_mutex_init(&list->size_lock, NULL);
	pthread_mutex_init(&list->head_lock, NULL);
	rw_lock_init(&list->lock);
}


static void free_aux(linked_list_t* list){
	assert(list);
	node *current = list->head, *next = NULL;
	while (current) {
		next = current->next;
//		rw_lock_destroy(&current->lock);
		free(current);
		current = next;
	}
#ifndef NDEBUG
	pthread_mutex_destroy(&list->size_lock);
	pthread_mutex_destroy(&list->head_lock);
#else
	assert(pthread_mutex_destroy(&list->size_lock));
	assert(pthread_mutex_destroy(&list->head_lock));
#endif
}

/*----------------------------Threaded functions wrapper----------------------*/

//nothing seems to be critical here
static void* run_op(void* list_and_params) {
	//TODO remove this
	assert(list_and_params);

	list_params_t* params = (list_params_t*) list_and_params;

	assert(params->list && params->op_param);

	op_t* op = params->op_param;
	linked_list_t* list = params->list;

	switch (op->op) {
	case INSERT:
		op->result = list_insert(list, op->key, op->data);
		break;
	case REMOVE:
		op->result = list_remove(list, op->key);
		break;
	case CONTAINS:
		op->result = list_find(list, op->key);
		break;
	case UPDATE:
		op->result = list_update(list, op->key, op->data);
		break;
	case COMPUTE:
		op->result = list_compute(list, op->key, op->compute_func, op->data);
		break;
	default:
		assert(0);
	}
	return NULL; //since we have to return something
}

/**---------------------------- Interface functions --------------------------*/

linked_list_t* list_alloc() {
	linked_list_t* new_list;
	MALLOC_ORELSE(new_list, return NULL);

	list_init(new_list);
	return new_list;
}

void list_free(linked_list_t* list) {
	if (!list)
		return;

	if(!write_lock(&list->lock)){
		return;
	}
	//now no one can access the list
	free_aux(list);
	write_unlock(&list->lock);
	//TODO what if somebody enters here? Whatever
	rw_lock_destroy(&list->lock);
	free(list);
}

int list_split(linked_list_t* list, int n, linked_list_t** arr) {
	if (!list || !arr)
		return NULL_ARG;
	if (n <= 0)
		return INVALID_ARG;

	if(!write_lock(&list->lock))
		return DESTROY_PENDING;

	for (int i = 0; i < n; i++) {
		arr[i] = list_alloc(); // TODO check
		list_init(arr[i]);
	}

	node* current = list->head;
	int i = 0;
	while (current) {
		list_insert(arr[i], current->key, current->data);
		i = (i + 1) % n;
		current = current->next;
	}
	free_aux(list);

	write_unlock(&list->lock);
	//TODO what if somebody enters here?
	rw_lock_destroy(&list->lock);
	free(list);
	return SUCCESS;
}

int list_insert(linked_list_t* list, int key, void* data) {
	if (!list)
		return NULL_ARG;

	if(!read_lock(&list->lock))
		return DESTROY_PENDING;

	int res = SUCCESS;
	node* new_node;
	MALLOC_ORELSE(new_node, res=MEM_ERROR; goto unlock_rw);
	init_node(new_node, key, data);

	node* prev = closest_below_key(list, key);
	if (!prev){ // head_lock and (if exists) 1st node are locked
		node* first_in_list = list->head;
		if(list->head && list->head->key == key) {
			pthread_mutex_unlock(&list->head_lock);
			pthread_mutex_unlock(&first_in_list->lock);
			destroy_node(new_node);
			res = ALREADY_IN_LIST;
			goto unlock_rw;
		}
		insert_first(list, new_node);
		pthread_mutex_unlock(&list->head_lock);
		if(first_in_list)
			pthread_mutex_unlock(&first_in_list->lock);
	}
	else{ // prev and prev->next (if exists) are locked
		node* tmp = prev->next;
		if(prev->next && prev->next->key == key) {
			pthread_mutex_unlock(&prev->lock);
			pthread_mutex_unlock(&tmp->lock);
			destroy_node(new_node);
			res = ALREADY_IN_LIST;
			goto unlock_rw;
		}
		insert_after(prev, new_node);
		pthread_mutex_unlock(&prev->lock);
		if(tmp)
			pthread_mutex_unlock(&tmp->lock);
	}

	pthread_mutex_lock(&list->size_lock);
	list->size++;
	pthread_mutex_unlock(&list->size_lock);

unlock_rw:
	read_unlock(&list->lock);
	return res;
}

int list_remove(linked_list_t* list, int key) {
	if (!list)
		return NULL_ARG;

	if(!read_lock(&list->lock))
		return DESTROY_PENDING;

	int res = SUCCESS;

	node* prev = closest_below_key(list, key);//returns with locks

	if (prev && prev->next) { //both locked now
		node* tmp = prev->next;
		if(prev->next->key == key){
			remove_after(prev);
			pthread_mutex_unlock(&prev->lock);
			//no unlock for prev->next (which has been removed)
		} else {
			pthread_mutex_unlock(&prev->lock);
			pthread_mutex_unlock(&tmp->lock);
			res = NOT_FOUND;
			goto rw_unlock;
		}
	} else if (prev && !prev->next) {
		// last node returned from closest_below_key
		pthread_mutex_unlock(&prev->lock);
		res = NOT_FOUND;
		goto rw_unlock;
	} 	else if (list->head) {
		// prev == NULL, but there's at least 1 node in list, and then 1st node is locked too
		node* tmp = list->head;
		if(list->head->key == key){
			remove_first(list);
			pthread_mutex_unlock(&list->head_lock);
			//no unlock for tmp (which has been removed)
		} else {
			pthread_mutex_unlock(&list->head_lock);
			pthread_mutex_unlock(&tmp->lock);
			res = NOT_FOUND;
			goto rw_unlock;
		}
	} else { // list empty
		pthread_mutex_unlock(&list->head_lock);
		res = NOT_FOUND;
		goto rw_unlock;
	}

	pthread_mutex_lock(&list->size_lock);
	list->size--;
	pthread_mutex_unlock(&list->size_lock);

rw_unlock:
	read_unlock(&list->lock);
	return res;
}

int list_find(linked_list_t* list, int key) {
	if (!list)
		return NULL_ARG;

	if(!read_lock(&list->lock))
		return DESTROY_PENDING;

	node* found = find(list, key);//if found, node returns locked
	if(found)
		pthread_mutex_unlock(&found->lock);

	read_unlock(&list->lock);

	return found != NULL;
}

int list_size(linked_list_t* list) {
	if (!list)
		return -NULL_ARG;

	if(!read_lock(&list->lock))
		return -DESTROY_PENDING;


	pthread_mutex_lock(&list->size_lock);
	int res = list->size;
	pthread_mutex_unlock(&list->size_lock);

	read_unlock(&list->lock);
	return res;
}

int list_update(linked_list_t* list, int key, void* data) {
	if (!list)
		return NULL_ARG;

	if(!read_lock(&list->lock))
		return DESTROY_PENDING;

	int res = SUCCESS;
	node* to_update = find(list, key);//if found, node returns locked
	if (!to_update){
		res = NOT_FOUND;
		goto unlock_rw;
	}

	to_update->data = data;
	pthread_mutex_unlock(&to_update->lock);

	unlock_rw:
	read_unlock(&list->lock);
	return res;
}

int list_compute(linked_list_t* list, int key,
		int (*compute_func)(void *), int* result) {
	if (!list || !result || !compute_func)
		return NULL_ARG;

	if(!read_lock(&list->lock))
		return DESTROY_PENDING;

	int res = SUCCESS;
	node* to_compute = find(list, key);//if found, node returns locked
	if (!to_compute){
		res = NOT_FOUND;
		goto unlock_rw;
	}

	*result = compute_func(to_compute->data);
	pthread_mutex_unlock(&to_compute->lock);

	unlock_rw:
	read_unlock(&list->lock);
	return res;
}

void list_batch(linked_list_t* list, int num_ops, op_t* ops) {
	if (!list || !ops || num_ops <= 0)
		return;
	pthread_t* threads;
	list_params_t* params;
	MALLOC_N_ORELSE(threads, num_ops, return);
	MALLOC_N_ORELSE(params, num_ops, free(threads); return);

	for (int i = 0; i < num_ops; i++) {
		params[i].list = list;
		params[i].op_param = &ops[i];
		pthread_create(&threads[i], NULL, run_op, &params[i]); //TODO do something in case of failure
	}
	for (int i = 0; i < num_ops; i++) {
#ifdef NDEBUG
		pthread_join(threads[i], NULL);
#else
		int r = pthread_join(threads[i], NULL);
		assert(r==0);
#endif
	}
	free(params);
	free(threads);
}

