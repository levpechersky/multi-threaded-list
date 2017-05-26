/*
 * my_list_test.c
 *
 *  Created on: 25 May 2017
 *      Author: Lev
 */

#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>// random
#include "my_list.h"

#undef RAND_MAX
#define RAND_MAX 20000

#define ASSERT_TEST(b) do { \
        if (!(b)) { \
                fprintf(stdout, "\nAssertion failed at %s:%d %s ",__FILE__,__LINE__,#b); \
                return false; \
        } \
} while (0)

#define RUN_TEST(test) do { \
        fprintf(stdout, "Testing "#test"... "); \
        if (test()) { \
            fprintf(stdout, "[OK]\n");\
        } else { \
        	fprintf(stdout, "[Failed]\n"); \
        } \
} while(0)

////////////////////////////////////////////////////////////////////////////////

static bool alloc_and_free() {
	linked_list_t* list = list_alloc();
	ASSERT_TEST(list!=NULL);
	list_free(NULL); // should not fail
	list_free(list);
	return true;
}

static bool insert_empty_list() {
	int key = 42, data = 4;
	linked_list_t* list = list_alloc();
	list_insert(list, key, &data);

	list_free(list);
	return true;
}

static bool insert_sorted_first() {
	int key_big = 20, data_big = 2;
	int key_small = 10, data_small = 1;
	linked_list_t* list = list_alloc();

	ASSERT_TEST(list_insert(list, key_big, &data_big) == 0);
	ASSERT_TEST(list_insert(list, key_small, &data_small) == 0);
	//TODO check order

	list_free(list);
	return true;
}

static bool insert_sorted_end() {
	int key_big = 20, data_big = 2;
	int key_small = 10, data_small = 1;
	linked_list_t* list = list_alloc();

	ASSERT_TEST(list_insert(list, key_small, &data_small) == 0);
	ASSERT_TEST(list_insert(list, key_big, &data_big) == 0);
	//TODO check order

	list_free(list);
	return true;
}

static bool insert_sorted_middle() {
	int key_big = 30, data_big = 3;
	int key_mid = 20, data_mid = 2;
	int key_small = 10, data_small = 1;
	linked_list_t* list = list_alloc();

	ASSERT_TEST(list_insert(list, key_small, &data_small) == 0);
	ASSERT_TEST(list_insert(list, key_big, &data_big) == 0);
	ASSERT_TEST(list_insert(list, key_mid, &data_mid) == 0);
	//TODO check order

	list_free(list);
	return true;
}

static bool find_empty_list() {
	linked_list_t* list = list_alloc();

	ASSERT_TEST(list_find(list, 1024) == 0);
	ASSERT_TEST(list_find(list, 0) == 0);
	ASSERT_TEST(list_find(list, -1024) == 0);

	list_free(list);
	return true;
}

static bool find_nonempty_list() {
	int key_big = 30, data_big = 3;
	int key_mid = 20, data_mid = 2;
	int key_small = 10, data_small = 1;
	linked_list_t* list = list_alloc();
	list_insert(list, key_small, &data_small);
	list_insert(list, key_big, &data_big);
	list_insert(list, key_mid, &data_mid);

	ASSERT_TEST(list_find(list, key_big) == 1);
	ASSERT_TEST(list_find(list, key_mid) == 1);
	ASSERT_TEST(list_find(list, key_small) == 1);
	ASSERT_TEST(list_find(list, 1024) == 0);
	ASSERT_TEST(list_find(list, 0) == 0);
	ASSERT_TEST(list_find(list, -1024) == 0);

	list_free(list);
	return true;
}

static bool size() {
	int keys_n = 10;
	int keys[] = { -29219, -24086, -10898, -6117, 2177, 11394, 11737, 16425,
			17654, 27198 };
	linked_list_t* list = list_alloc();
	for (int i = 0; i < keys_n; i++) {
		list_insert(list, keys[i], &keys[i]);
		ASSERT_TEST(list_size(list) == i + 1);
	}
	list_free(list);
	return true;
}

static bool remove_success() {
	int keys_n = 10;
	int keys[] = { -29219, -24086, -10898, -6117, 2177, 11394, 11737, 16425,
			17654, 27198 };
	linked_list_t* list = list_alloc();
	for (int i = 0; i < keys_n; i++) {
		list_insert(list, keys[i], &keys[i]);
	}
	for (int i = 0; i < keys_n; i++) {
		ASSERT_TEST(list_remove(list, keys[i]) == 0);
	}
	list_free(list);
	return true;
}

static bool remove_item_not_in_list() {
	int keys_n = 10, not_in_list_n = 8;
	int keys[] = { -29219, -24086, -10898, -6117, 2177, 11394, 11737, 16425,
			17654, 27198 };
	int not_in_list[] = { -1000000, -15000, -3000, 0, 2, 100, 15000, 1000000 };
	linked_list_t* list = list_alloc();
	for (int i = 0; i < keys_n; i++) {
		list_insert(list, keys[i], &keys[i]);
	}
	for (int i = 0; i < not_in_list_n; i++) {
		ASSERT_TEST(list_remove(list, not_in_list[i]) != 0);
	}
	list_free(list);
	return true;
}

/* For list_compute */
static int last_digit(void* p) {
	if (!p)
		return -1;
	int* x = (int*) p;
	*x = *x < 0 ? -*x : *x;
	*x %= 10;
	return 0;
}

/* For list_compute */
static int nullify(void* p) {
	if (!p)
		return -1;
	int* x = (int*) p;
	*x = 0;
	return 0;
}

static bool compute() {
	int keys_n = 10;
	int keys[] = { -29219, -24086, -10898, -6117, 2177, 11394, 11737, 16425,
			17654, 27198 }; // will be overwritten
	int results[] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 }; //will contain return values of compute
	int expected[] = { 9, 6, 8, 7, 7, 4, 7, 5, 4, 8 };
	linked_list_t* list = list_alloc();
	for (int i = 0; i < keys_n; i++) {
		list_insert(list, keys[i], &keys[i]);
	}
	for (int i = 0; i < keys_n; i++) {
		ASSERT_TEST(list_compute(list, keys[i], last_digit, &results[i]) == 0);
		ASSERT_TEST(results[i] == 0);
		ASSERT_TEST(keys[i] == expected[i]);
	}
	list_free(list);
	return true;
}

static bool update() {
	int keys_n = 10;
	int keys[] = { -29219, -24086, -10898, -6117, 2177, 11394, 11737, 16425,
			17654, 27198 };
	int new_keys[] = { 29617, 12264, -18099, -28722, 8327, -20500, -4249,
			-13822,
			11531, 17414 };
	int result; //
	linked_list_t* list = list_alloc();
	for (int i = 0; i < keys_n; i++) {
		list_insert(list, keys[i], &keys[i]);
	}
	for (int i = 0; i < keys_n; i++) {
		ASSERT_TEST(list_update(list, keys[i], &new_keys[i]) == 0);
		list_compute(list, keys[i], nullify, &result);
		ASSERT_TEST(result == 0);
		ASSERT_TEST(new_keys[i] == 0); //new data has been updated (with nullify)
		ASSERT_TEST(keys[i] != 0); // old data hasn't been updated
	}
	list_free(list);
	return true;
}

static bool split() {
	int keys_n = 10, split_into = 3;
	int keys[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 };
	linked_list_t* results[split_into];
	linked_list_t* list = list_alloc();
	for (int i = 0; i < keys_n; i++) {
		list_insert(list, keys[i], &keys[i]);
	}

	list_split(list, split_into, results);
	ASSERT_TEST(list_size(results[0]) == 4);
	ASSERT_TEST(list_size(results[1]) == 3);
	ASSERT_TEST(list_size(results[2]) == 3);

	//list_free(list); Don't need to
	for(int i=0; i<split_into; i++){
		list_free(results[i]);
	}
	return true;
}

//TODO need more batch tests
static bool batch_compute() {
	int keys_n = 10;
	int k1 = 12345, k2 = 234123;
	int keys[] = { k2, -24086, -10898, k1, 2177, 11394, 11737, 16425,
			17654, 27198 };
	int result;
	op_t param1 = { k1, &result, COMPUTE, nullify };
	op_t param2 = { k2, &result, COMPUTE, nullify };

	op_t params[] = { param1, param2 };

	linked_list_t* list = list_alloc();
	for (int i = 0; i < keys_n; i++) {
		list_insert(list, keys[i], &keys[i]);
	}

	list_batch(list, 2, params);
	ASSERT_TEST(keys[0] == 0);
	ASSERT_TEST(keys[3] == 0);

	list_free(list);
	return true;
}

static bool batch_inserts_few() {
	int keys_n = 6;
	int keys[] = { -1357, 9342, -26332, -22934, 16824, -14310 };
	int result;
	linked_list_t* list = list_alloc();
	op_t params[keys_n];

	for (int i = 0; i < keys_n; i++) {
		params[i].compute_func = NULL;
		params[i].data = &result;
		params[i].key = keys[i];
		params[i].op = INSERT;
		params[i].result = -1;
	}

	list_batch(list, keys_n, params);
	//TODO find a way to check result
	list_free(list);
	return true;
}

static bool batch_inserts_lots() {
	srand(time(NULL));
	int keys_n = 10000;
	int result;
	linked_list_t* list = list_alloc();
	op_t params[keys_n];

	for (int i = 0; i < keys_n; i++) {
		params[i].compute_func = NULL;
		params[i].data = &result;
		params[i].key = rand();
		params[i].op = INSERT;
		params[i].result = -1;
	}

	list_batch(list, keys_n, params);
	//TODO find a way to check result
	list_free(list);
	return true;
}

static bool batch_insert_remove() {
	srand(3003);
	int keys_n = 10000;
	int result;
	linked_list_t* list = list_alloc();
	op_t params[keys_n];

	for (int i = 0; i < keys_n; i+=2) {
		params[i].compute_func = NULL;
		params[i].data = &result;
		params[i].key = rand();
		params[i].op = INSERT;
		params[i].result = -1;
	}

	for (int i = 1; i < keys_n; i+=2) {
		params[i].compute_func = NULL;
		params[i].data = &result;
		params[i].key = rand();
		params[i].op = REMOVE;
		params[i].result = -1;
	}

	list_batch(list, keys_n, params);
	//TODO find a way to check result
	list_free(list);
	return true;
}



int main(int argc, char** argv) {
	RUN_TEST(alloc_and_free);
	RUN_TEST(insert_empty_list);
	RUN_TEST(insert_sorted_first);
	RUN_TEST(insert_sorted_end);
	RUN_TEST(insert_sorted_middle);
	RUN_TEST(find_empty_list);
	RUN_TEST(find_nonempty_list);
	RUN_TEST(size);
	RUN_TEST(remove_success);
	RUN_TEST(remove_item_not_in_list);
	RUN_TEST(compute);
	RUN_TEST(update);
	RUN_TEST(split);
	RUN_TEST(batch_compute);
	RUN_TEST(batch_inserts_few);
	RUN_TEST(batch_inserts_lots);
	RUN_TEST(batch_insert_remove);
	return 0;
}

