/*
 * my_list.c
 *
 *  Created on: 25 May 2017
 *      Author: Lev
 */
#include <stdlib.h>
#include <limits.h>
#include <assert.h>
#include <pthread.h>
#include <stdio.h>
#include <sys/types.h>
#include "rwlock.h" // FIXME, must copy-paste it here for an assignment

#include "my_list.h"

/*------------------------- Types and definitions-----------------------------*/

typedef struct node_t {
	int key;
	void* data;
	struct node_t* next;
	rwlock_t lock;
} node;

struct linked_list_t {
	node* head;
	int size;
	rwlock_t lock;
};

typedef struct list_params_t {
	linked_list_t* list;
	op_t* op_param;
} list_params_t;

enum list_error {
	SUCCESS = 0,
	NULL_ARG,
	INVALID_ARG,
	MEM_ERROR,
	NOT_FOUND,
	ALREADY_IN_LIST
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
	rw_lock_init(&new_node->lock);
}

static inline void insert_first(linked_list_t* list, node* new_node) {
	assert(list && new_node);
	if (list->head) {
		new_node->next = list->head;
	}
	list->head = new_node;
}

static inline void insert_after(node* previous, node* new_node) {
	assert(previous && new_node);
	new_node->next = previous->next;
	previous->next = new_node;
}

static inline void remove_first(linked_list_t* list) {
	assert(list && list->head);
	node* to_remove = list->head;
	list->head = to_remove->next;
	free(to_remove);
}

static inline void remove_after(node* previous) {
	assert(previous && previous->next);
	node* to_remove = previous->next;
	previous->next = previous->next->next;
	free(to_remove);
}

/* Return pointer to node v, where v.key < key. If for each node
 * node.key >= key (i.e. node with key should be 1st), returns NULL
 * (including the case when list is empty) */
static inline node* closest_below_key(linked_list_t* list, int key) {
	assert(list);
	node *prev = NULL, *current = list->head;
	while (current && current->key < key) {
		prev = current;
		current = current->next;
	}
	return prev;
}

static inline node* find(linked_list_t* list, int key) {
	assert(list);
	node* prev = closest_below_key(list, key);
	if (prev && prev->next && prev->next->key == key)
		return prev->next;
	else if (list->head && list->head->key == key)
		return list->head;
	else
		return NULL;
}

static inline void list_init(linked_list_t* list) {
	assert(list);
	list->head = NULL;
	list->size = 0;
	rw_lock_init(&list->lock);
}

/*----------------------------Threaded functions wrapper----------------------*/
//nothing seems to be critical here
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

	//lock all?
	node *current = list->head, *next = NULL;
	while (current) {
		next = current->next;
		rw_lock_destroy(&current->lock);
		free(current);
		current = next;
	}
	rw_lock_destroy(&list->lock);
	free(list);
}

int list_split(linked_list_t* list, int n, linked_list_t** arr) {
	if (!list || !arr)
		return NULL_ARG;
	if (n <= 0)
		return INVALID_ARG;

	for (int i = 0; i < n; i++) {
		arr[i] = list_alloc(); // TODO check
		list_init(arr[i]);
	}

	//lock all?
	node* current = list->head;
	int i = 0;
	while (current) {
		list_insert(arr[i], current->key, current->data);
		i = (i + 1) % n;
		current = current->next;
	}
	list_free(list);
	return SUCCESS;
}

int list_insert(linked_list_t* list, int key, void* data) {
	if (!list)
		return NULL_ARG;
	node* new_node;
	MALLOC_ORELSE(new_node, return MEM_ERROR);
	init_node(new_node, key, data);

	node* prev = closest_below_key(list, key);
	if (!prev){
		if(list->head && list->head->key == key)
			return ALREADY_IN_LIST;
		insert_first(list, new_node);
	}
	else{
		if(prev->next && prev->next->key == key)
			return ALREADY_IN_LIST;
		insert_after(prev, new_node);
	}
	list->size++;
	return SUCCESS;
}

int list_remove(linked_list_t* list, int key) {
	if (!list)
		return NULL_ARG;

	node* prev = closest_below_key(list, key);
	if (prev && prev->next && prev->next->key == key)
		remove_after(prev);
	else if (list->head && list->head->key == key)
		remove_first(list);
	else
		return NOT_FOUND;

	list->size--;
	return SUCCESS;
}

int list_find(linked_list_t* list, int key) {
	if (!list)
		return NULL_ARG;

	node* found = find(list, key);
	return found != NULL;
}

int list_size(linked_list_t* list) {
	if (!list)
		return NULL_ARG;

	return list->size;
}

int list_update(linked_list_t* list, int key, void* data) {
	if (!list)
		return NULL_ARG;

	node* to_update = find(list, key);
	if (!to_update)
		return NOT_FOUND;

	to_update->data = data;

	return SUCCESS;
}

int list_compute(linked_list_t* list, int key,
		int (*compute_func)(void *), int* result) {
	if (!list || !result || !compute_func)
		return NULL_ARG;

	node* to_compute = find(list, key);
	if (!to_compute)
		return NOT_FOUND;

	*result = compute_func(to_compute->data);
	return SUCCESS;
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
		assert(pthread_join(threads[i], NULL) == 0);
#endif
	}
	free(params);
	free(threads);
}

