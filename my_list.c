/*
 * my_list.c
 *
 *  Created on: 25 May 2017
 *      Author: Lev
 */

#include "my_list.h"

#include <assert.h>
#include <pthread.h>
#include <stdlib.h>

/*------------------------- Lock types and definitions -----------------------*/

typedef pthread_mutex_t mutex_t;

/* Readers-cleaner lock. Like readers-writers lock, but unlike writers,
 * only 1 cleaner is allowed to hold or wait for lock (because if cleaner gets
 * the lock, or waits for it, it means that object protected by lock is about
 * to be destroyed).  While cleaner holds or waits for the lock, for every other
 * reader/cleaner actions of acquiring the lock will fail (and return 0).
 * Cleaner never releases the lock (since lock is destroyed during cleanup). */
typedef struct rc_lock {
	int number_of_readers, cleaning_pending;
	pthread_cond_t cleaner_condition;
	mutex_t global_lock;
} rc_lock_t;

void rc_lock_init(rc_lock_t* lock) {
	assert(lock);
	lock->number_of_readers = 0;
	lock->cleaning_pending = 0;
	pthread_cond_init(&lock->cleaner_condition, NULL);
	pthread_mutex_init(&lock->global_lock, NULL);
}

void rc_lock_destroy(rc_lock_t* lock) {
	assert(lock);
	pthread_cond_destroy(&lock->cleaner_condition);
	pthread_mutex_destroy(&lock->global_lock);
}

/* @Return:
 *   0 - if there's cleaner waiting, or cleaning in progress.
 *       Doesn't aquire lock in this case.
 *   1 - if successfully aquired the lock.
 */
int read_lock(rc_lock_t* lock) {
	assert(lock);
	int res = 1;
	pthread_mutex_lock(&lock->global_lock);
	if (lock->cleaning_pending)
		res = 0;
	else
		lock->number_of_readers++;
	pthread_mutex_unlock(&lock->global_lock);
	return res;
}

void read_unlock(rc_lock_t* lock) {
	assert(lock);
	pthread_mutex_lock(&lock->global_lock);
	lock->number_of_readers--;
	if (lock->number_of_readers == 0)
		pthread_cond_signal(&lock->cleaner_condition);
	pthread_mutex_unlock(&lock->global_lock);
}

/* @Return:
 *   0 - if there's cleaner waiting, or cleaning in progress.
 *       Doesn't aquire lock in this case.
 *   1 - if successfully aquired the lock.
 */
int cleanup_lock(rc_lock_t* lock) {
	assert(lock);
	int res = 1;
	pthread_mutex_lock(&lock->global_lock);
	if (lock->cleaning_pending) {
		res = 0;
	} else {
		lock->cleaning_pending = 1;
		while (lock->number_of_readers > 0)
			pthread_cond_wait(&lock->cleaner_condition, &lock->global_lock);
	}
	pthread_mutex_unlock(&lock->global_lock);
	return res;
}

/* NULL-safe versions of pthread_mutex_lock and pthread_mutex_unlock.
 * If lock is NULL, those functions have no effect.
 * Not suitable for every case.
 */
int mutex_lock_safe(mutex_t* lock){
	return lock ? pthread_mutex_lock(lock) : 0;
}
int mutex_unlock_safe(mutex_t* lock){
	return lock ? pthread_mutex_unlock(lock) : 0;
}

/*-------------------------List types and definitions-------------------------*/

typedef struct node_t {
	int key;
	void* data;
	struct node_t* next;
	mutex_t lock;
} node_t;

struct linked_list_t {
	node_t* head;
	int size;
	rc_lock_t cleanup_lock;
	mutex_t size_lock, head_ptr_lock;
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
	CLEANUP_PENDING
};

#define MALLOC_N_ORELSE(identifier, N, command) do {\
	identifier = malloc(sizeof(*(identifier))*(N)); \
	if(!(identifier)) { \
	command; \
	} } while(0)

#define MALLOC_ORELSE(identifier, command) \
		MALLOC_N_ORELSE(identifier, 1, command)

/*------------------------- Static helper functions --------------------------*/

static void init_node(node_t* new_node, int key, void* data) {
	assert(new_node);
	new_node->key = key;
	new_node->data = data;
	new_node->next = NULL;
	pthread_mutex_init(&new_node->lock, NULL);
}

/* node should be inaccessible for other threads and unlocked */
static void destroy_node(node_t* to_destroy) {
	assert(to_destroy);
	pthread_mutex_destroy(&to_destroy->lock);
	free(to_destroy);
}

/* required locks: head */
static void insert_first(linked_list_t* list, node_t* new_node) {
	assert(list && new_node);
	new_node->next = list->head;
	list->head = new_node;
}

/* required locks: previous, previous->next */
static void insert_after(node_t* previous, node_t* new_node) {
	assert(previous && new_node);
	new_node->next = previous->next;
	previous->next = new_node;
}

/* required locks: head, 1st node */
static void remove_first(linked_list_t* list) {
	assert(list && list->head);
	node_t* to_remove = list->head;
	list->head = to_remove->next;

	pthread_mutex_unlock(&to_remove->lock);
	destroy_node(to_remove);
}

/* required locks: previous, previous->next */
static void remove_after(node_t* previous) {
	assert(previous && previous->next);
	node_t* to_remove = previous->next;
	previous->next = to_remove->next;

	pthread_mutex_unlock(&to_remove->lock);
	destroy_node(to_remove);
}

/* Return pointer to node v, where v.key < key. If for each node
 * node.key >= key (i.e. node with key should be 1st), returns NULL
 * (including the case when list is empty).
 * Returns pointers to locks it acquired, in output variables prev_lock and next_lock.
 *
 * Uses hand-over-hand locking. Upon calling no node has to be locked.
 *
 * Locks info (upon return):
 * if returns last node - last node locked
 * if returns NULL (only head is below key) - locks head and (if exists) the 1st node
 * otherwise locks closest below and next to it
 */
static node_t* closest_below_key(linked_list_t* list, int key,
		mutex_t** prev_lock, mutex_t** next_lock) {
	assert(list && prev_lock && next_lock);
	*prev_lock = &list->head_ptr_lock;
	*next_lock = NULL;

	pthread_mutex_lock(&list->head_ptr_lock);
	node_t *prev = NULL, *current = list->head;
	if (list->head) {
		pthread_mutex_lock(&list->head->lock);
		*next_lock = &list->head->lock;
	}
	while (current && current->key < key) { /* we enter here only if there's at least 1 node */
		pthread_mutex_unlock(*prev_lock);
		prev = current;
		*prev_lock = *next_lock;
		current = current->next;
		if (current)
			pthread_mutex_lock(&current->lock); /* updated current, i.e. next node */
		*next_lock = current ? &current->lock : NULL;
	}
	return prev;
}

/* Returns with lock on found node only, or without any lock,
 * if node with key not found.
 * Required locks - none.
 */
static node_t* find(linked_list_t* list, int key) {
	assert(list);
	node_t* found = NULL;
	mutex_t *prev_lock, *next_lock;

	node_t* prev = closest_below_key(list, key, &prev_lock, &next_lock);
	if (prev && prev->next && prev->next->key == key) { /* both locked now */
		found = prev->next;
	} else if (!prev && list->head && list->head->key == key) {
		/*  prev == NULL, but there's at least 1 node in list, and then 1st node is locked too */
		found = list->head;
	}
	mutex_unlock_safe(prev_lock);
	if (!found)
		mutex_unlock_safe(next_lock);
	return found;
}

static void list_init(linked_list_t* list) {
	assert(list);
	list->head = NULL;
	list->size = 0;
	pthread_mutex_init(&list->size_lock, NULL);
	pthread_mutex_init(&list->head_ptr_lock, NULL);
	rc_lock_init(&list->cleanup_lock);
}

/* required locks: cleanup_lock */
static void list_cleanup(linked_list_t* list) {
	assert(list);
	node_t *current = list->head, *next = NULL;
	while (current) {
		next = current->next;
		free(current);
		current = next;
	}
	pthread_mutex_destroy(&list->size_lock);
	pthread_mutex_destroy(&list->head_ptr_lock);
}

static int alloc_and_init_list_array(int n, linked_list_t** arr) {
	int i = 0, j;
	for (; i < n; i++) {
		arr[i] = list_alloc();
		if (arr[i] == NULL)
			goto cleanup;
		list_init(arr[i]);
	}
	return SUCCESS;

cleanup:
	for (j = 0; j < i; j++)
		list_free(arr[j]);
	return MEM_ERROR;
}

/*----------------------------Threaded functions wrapper----------------------*/

static void* run_op(void* list_and_params) {
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
	return NULL; /* since we have to return something */
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
	if (!cleanup_lock(&list->cleanup_lock))
		return;

	list_cleanup(list);
	rc_lock_destroy(&list->cleanup_lock);
	free(list);
}

int list_split(linked_list_t* list, int n, linked_list_t** arr) {
	if (!list || !arr)
		return NULL_ARG;
	if (n <= 0)
		return INVALID_ARG;

	if(alloc_and_init_list_array(n, arr) != SUCCESS)
		return MEM_ERROR;

	if (!cleanup_lock(&list->cleanup_lock))
		return CLEANUP_PENDING;

	/* Now no one can access the list, so we bypass nodes locks */
	node_t* current = list->head;
	int i = 0;
	while (current) {
		list_insert(arr[i], current->key, current->data);
		i = (i + 1) % n;
		current = current->next;
	}
	list_cleanup(list);
	rc_lock_destroy(&list->cleanup_lock);
	free(list);
	return SUCCESS;
}

int list_insert(linked_list_t* list, int key, void* data) {
	if (!list)
		return NULL_ARG;
	if (!read_lock(&list->cleanup_lock))
		return CLEANUP_PENDING;
	/*Now no one can access the list, so we bypass nodes locks*/

	int res = SUCCESS;
	mutex_t *prev_lock, *next_lock;
	node_t* new_node;
	MALLOC_ORELSE(new_node, res=MEM_ERROR; goto unlock_rw);
	init_node(new_node, key, data);

	node_t* prev = closest_below_key(list, key, &prev_lock, &next_lock);
	if ((prev && prev->next && prev->next->key == key)
			|| (!prev && list->head && list->head->key == key)) {
		destroy_node(new_node);
		res = ALREADY_IN_LIST;
		goto unlock_prev_next;
	}
	if (!prev)  /* head_lock and (if exists) 1st node are locked*/
		insert_first(list, new_node);
	else		/* prev and prev->next (if exists) are locked*/
		insert_after(prev, new_node);

unlock_prev_next:
	mutex_unlock_safe(prev_lock);
	mutex_unlock_safe(next_lock);
	if (res == SUCCESS) {
		pthread_mutex_lock(&list->size_lock);
		list->size++;
		pthread_mutex_unlock(&list->size_lock);
	}

unlock_rw:
	read_unlock(&list->cleanup_lock);
	return res;
}

int list_remove(linked_list_t* list, int key) {
	if (!list)
		return NULL_ARG;
	if (!read_lock(&list->cleanup_lock))
		return CLEANUP_PENDING;

	int res = SUCCESS;
	mutex_t *prev_lock, *next_lock;
	node_t* prev = closest_below_key(list, key, &prev_lock, &next_lock);
	if ((prev && !prev->next) || (prev && prev->next && prev->next->key != key)
			|| (!prev && !list->head)
			|| (!prev && list->head && list->head->key != key)) {
		res = NOT_FOUND;
		goto unlock_prev_next;
	}
	if (!prev)  /* head_lock and 1st node are locked*/
		remove_first(list);
	else 	   /* prev and prev->next are locked*/
		remove_after(prev);

unlock_prev_next:
	mutex_unlock_safe(prev_lock);
	if (res != SUCCESS) {
		mutex_unlock_safe(next_lock); /* if successfully deleted - this lock doesn't exist anymore */
	} else {
		pthread_mutex_lock(&list->size_lock);
		list->size--;
		pthread_mutex_unlock(&list->size_lock);
	}
	read_unlock(&list->cleanup_lock);
	return res;
}

int list_find(linked_list_t* list, int key) {
	if (!list)
		return NULL_ARG;
	if (!read_lock(&list->cleanup_lock))
		return CLEANUP_PENDING;

	node_t* found = find(list, key); /*if found, node returns locked */
	if (found)
		pthread_mutex_unlock(&found->lock);

	read_unlock(&list->cleanup_lock);

	return found != NULL;
}

int list_size(linked_list_t* list) {
	if (!list)
		return -NULL_ARG;

	if (!read_lock(&list->cleanup_lock))
		return -CLEANUP_PENDING;

	pthread_mutex_lock(&list->size_lock);
	int res = list->size;
	pthread_mutex_unlock(&list->size_lock);

	read_unlock(&list->cleanup_lock);
	return res;
}

int list_update(linked_list_t* list, int key, void* data) {
	if (!list)
		return NULL_ARG;
	if (!read_lock(&list->cleanup_lock))
		return CLEANUP_PENDING;

	int res = SUCCESS;
	node_t* to_update = find(list, key); /*if found, node returns locked */
	if (!to_update) {
		res = NOT_FOUND;
		goto unlock_rw;
	}
	to_update->data = data;
	pthread_mutex_unlock(&to_update->lock);

unlock_rw:
	read_unlock(&list->cleanup_lock);
	return res;
}

int list_compute(linked_list_t* list, int key,
		int (*compute_func)(void *), int* result) {
	if (!list || !result || !compute_func)
		return NULL_ARG;
	if (!read_lock(&list->cleanup_lock))
		return CLEANUP_PENDING;

	int res = SUCCESS;
	node_t* to_compute = find(list, key); /*if found, node returns locked */
	if (!to_compute) {
		res = NOT_FOUND;
		goto unlock_rw;
	}
	*result = compute_func(to_compute->data);
	pthread_mutex_unlock(&to_compute->lock);

unlock_rw:
	read_unlock(&list->cleanup_lock);
	return res;
}

void list_batch(linked_list_t* list, int num_ops, op_t* ops) {
	int i;
	if (!list || !ops || num_ops <= 0)
		return;
	pthread_t* threads;
	list_params_t* params;
	MALLOC_N_ORELSE(threads, num_ops, return);
	MALLOC_N_ORELSE(params, num_ops, free(threads); return);

	for (i = 0; i < num_ops; i++) {
		params[i].list = list;
		params[i].op_param = &ops[i];
		pthread_create(&threads[i], NULL, run_op, &params[i]); /*TODO do something in case of failure */
	}
	for (i = 0; i < num_ops; i++) {
		pthread_join(threads[i], NULL); /* TODO should I check exit status? */
	}
	free(params);
	free(threads);
}

