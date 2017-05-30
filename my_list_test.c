/*
 * my_list_test.c
 *
 *  Created on: 25 May 2017
 *      Author: Lev
 */
#ifdef XXX

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
	srand(3003);
	int keys_n = 3000;
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


#endif


#include "my_list.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#define LIST_FOR_EACH(list) for(int i = 0; i < list_size((list)) ; ++i)



#define ASSERT_NON_ZERO(b) do { \
if ((b) == 0) { \
	fprintf(stdout, "\nAssertion failed at %s:%d %s ",__FILE__,__LINE__,#b); \
	return false; \
} \
} while (0)

#define ASSERT_ZERO(b) do { \
if ((b) != 0) { \
	fprintf(stdout, "\nAssertion failed at %s:%d %s ",__FILE__,__LINE__,#b); \
	return false; \
} \
} while (0)

#define ASSERT_TEST(b) do { \
if (!(b)) { \
	fprintf(stdout, "\nAssertion failed at %s:%d %s ",__FILE__,__LINE__,#b); \
	return false; \
} \
} while (0)

#define RUN_TEST(test) do { \
fprintf(stdout, "Running "#test"... "); \
if (test()) { \
	fprintf(stdout, "[OK]\n");\
} else { \
	fprintf(stdout, "[Failed]\n"); \
} \
} while(0)

static bool isAEIOU(char ch){
	return 	((ch =='a') || (ch == 'A')
			||(ch =='e') || (ch == 'E')
			||(ch =='i') || (ch == 'I')
			||(ch =='o') || (ch == 'O')
			||(ch =='u') || (ch == 'U'));
}
static int youComputeNothing(void* data){
	char* name = (char*)data;
	int count = 0;
	for(int i=0;i<strlen(name);++i)
		count+= isAEIOU(name[i]);
	return count;
}

static int randRange(int end){
	return rand() % end;
}

bool testFreeErrors(){
	list_free(NULL);
	linked_list_t* list = list_alloc();
	list_free(list);

	return true;
}

bool testSplitErrors(){
	linked_list_t* list = list_alloc();
	ASSERT_NON_ZERO(list_split(NULL,1984,NULL));
	ASSERT_NON_ZERO(list_split(list,1984,NULL));
	ASSERT_NON_ZERO(list_split(list,0,NULL));
	ASSERT_NON_ZERO(list_split(list,-1984,NULL));

	linked_list_t* arr[5];
	ASSERT_NON_ZERO(list_split(list,-1984,arr));

	ASSERT_NON_ZERO(list_split(NULL,1984,arr));
	ASSERT_NON_ZERO(list_split(NULL,0,arr));
	ASSERT_NON_ZERO(list_split(NULL,-1984,arr));

	list_free(list);
	return true;
}

bool testInsertErrors(){
	linked_list_t* list = list_alloc();
	int data = 42;
	ASSERT_NON_ZERO(list_insert(NULL,1984,NULL));
	ASSERT_NON_ZERO(list_insert(NULL,1984,&data));

	ASSERT_ZERO(list_insert(list,1984,&data));
	ASSERT_NON_ZERO(list_insert(list,1984,&data));
	ASSERT_NON_ZERO(list_insert(list,1984,&data));
	list_free(list);
	return true;
}

bool testRemoveErrors(){
	linked_list_t* list = list_alloc();
	int data = 42;
	ASSERT_NON_ZERO(list_remove(NULL,1984));

	ASSERT_NON_ZERO(list_remove(list,1984));

	list_free(list);
	return true;
}

bool testFindErrors(){
	linked_list_t* list = list_alloc();
	int data = 42;
	ASSERT_TEST(list_find(NULL,1984) != 0);

	ASSERT_TEST(list_find(list,1984) == 0);
	list_free(list);
	return true;
}

bool testSizeErrors(){
	//TODO: PIAZZA - check about list_size(NULL)
	linked_list_t* list = list_alloc();

	list_free(list);
	return true;
}

bool testUpdateErrors(){
	linked_list_t* list = list_alloc();
	ASSERT_NON_ZERO(list_update(NULL,1984,NULL));
	ASSERT_NON_ZERO(list_update(NULL,0,NULL));
	ASSERT_NON_ZERO(list_update(NULL,-1984,NULL));

	ASSERT_NON_ZERO(list_update(list,-1984,NULL));

	list_free(list);
	return true;
}

bool testComputeErrors(){
	linked_list_t* list = list_alloc();
	int result;
	ASSERT_NON_ZERO(list_compute(NULL,1984,NULL,NULL));
	ASSERT_NON_ZERO(list_compute(NULL,1984,youComputeNothing,NULL));
	ASSERT_NON_ZERO(list_compute(NULL,1984,youComputeNothing,&result));
	ASSERT_NON_ZERO(list_compute(NULL,1984,NULL,&result));
	ASSERT_NON_ZERO(list_compute(list,1984,NULL,NULL));
	ASSERT_NON_ZERO(list_compute(list,1984,youComputeNothing,NULL));
	ASSERT_NON_ZERO(list_compute(list,1984,NULL,&result));

	list_free(list);
	return true;
}

bool testBatchErrors(){
	linked_list_t* list = list_alloc();
	op_t ops;
	list_batch(NULL,1984,NULL);
	list_batch(NULL,1984,&ops);

	list_batch(NULL,-1984,NULL);
	list_batch(NULL,-1984,&ops);
	list_batch(list,-1984,NULL);
	list_batch(list,-1984,&ops);

	list_free(list);
	return true;
}

bool testSequential1(){
	linked_list_t* list1 = list_alloc();
	linked_list_t* list2 = list_alloc();
	linked_list_t* list3 = list_alloc();
	int numOfStarks = 6 , numOfLannisters = 4;
	int allStarks[6] = {66,22,55,11,44,33};
	int n1 = 1 ,n2 = 3, n3 = numOfLannisters;
	linked_list_t* arr1[n1];
	linked_list_t* arr2[n2];
	linked_list_t* arr3[numOfLannisters];
	ASSERT_TEST(list_size(list1) == 0);
	ASSERT_ZERO(list_insert(list1,66,"Jon"));
	ASSERT_ZERO(list_insert(list1,44,"Sansa"));
	ASSERT_ZERO(list_insert(list1,55,"Arya"));
	ASSERT_ZERO(list_insert(list1,22,"Bran"));
	ASSERT_ZERO(list_insert(list1,11,"Rickon"));
	ASSERT_ZERO(list_insert(list1,33,"Robb"));
	ASSERT_TEST(list_size(list1) == numOfStarks);

	ASSERT_ZERO(list_insert(list2,444,"Joffrey"));
	ASSERT_ZERO(list_insert(list2,333,"Tommen"));
	ASSERT_ZERO(list_insert(list2,111,"Myrcella"));
	ASSERT_ZERO(list_insert(list2,222,"Tywin"));
	ASSERT_TEST(list_size(list2) == numOfLannisters);

	ASSERT_ZERO(list_insert(list3,444,"Joffrey"));
	ASSERT_ZERO(list_insert(list3,111,"Myrcella"));
	ASSERT_TEST(list_size(list3) == (numOfLannisters-2));

	LIST_FOR_EACH(list1){
		ASSERT_TEST(list_find(list1,allStarks[i]) == 1);
	}

	ASSERT_ZERO(list_remove(list1,33));
	ASSERT_ZERO(list_remove(list1,11));
	ASSERT_TEST(list_size(list1) == (numOfStarks-2));
	ASSERT_TEST(list_find(list1,33) == 0);
	ASSERT_TEST(list_find(list1,11) == 0);

	ASSERT_ZERO(list_insert(list1,11,"Rickon"));
	ASSERT_ZERO(list_insert(list1,33,"Robb"));
	ASSERT_TEST(list_find(list1,33) == 1);
	ASSERT_TEST(list_find(list1,11) == 1);

	ASSERT_ZERO(list_split(list1,n1,arr1)); // list1 is freed
	ASSERT_TEST(list_size(arr1[0]) == numOfStarks); // arr1[0] is identical to list1 before list_split

	ASSERT_ZERO(list_split(list2,n2,arr2)); // list2 is freed
	ASSERT_TEST(list_find(arr2[0],111) == 1);
	ASSERT_TEST(list_find(arr2[0],444) == 1);
	ASSERT_TEST(list_size(arr2[0]) == 2);
	ASSERT_TEST(list_find(arr2[1],222) == 1);
	ASSERT_TEST(list_size(arr2[1]) == 1);
	ASSERT_TEST(list_find(arr2[2],333) == 1);
	ASSERT_TEST(list_size(arr2[2]) == 1);

	ASSERT_ZERO(list_split(list3,n3,arr3)); // list3 is freed
	ASSERT_TEST(list_find(arr3[0],111) == 1);
	ASSERT_TEST(list_find(arr3[1],444) == 1);
	ASSERT_TEST(list_size(arr3[0]) == 1);
	ASSERT_TEST(list_size(arr3[1]) == 1);
	ASSERT_TEST(list_size(arr3[2]) == 0); // empty list
	ASSERT_TEST(list_size(arr3[2]) == 0); // empty list


	// free all allocated lists
	for(int i=0;i<n1;++i)
		list_free(arr1[i]);
	for(int i=0;i<n2;++i)
		list_free(arr2[i]);
	for(int i=0;i<n3;++i)
		list_free(arr3[i]);
	return true;
}

bool testSequential2(){
	linked_list_t* list1 = list_alloc();
	linked_list_t* list2 = list_alloc();
	linked_list_t* list3 = list_alloc();
	int result;

	ASSERT_ZERO(list_insert(list1,66,"Jon"));
	ASSERT_ZERO(list_insert(list1,44,"Sansa"));
	ASSERT_ZERO(list_insert(list1,55,"Arya"));
	ASSERT_ZERO(list_insert(list1,22,"Bran"));
	ASSERT_ZERO(list_insert(list1,11,"Rickon"));
	ASSERT_ZERO(list_insert(list1,33,"Robb"));

	ASSERT_ZERO(list_insert(list2,444,"Joffrey"));
	ASSERT_ZERO(list_insert(list2,333,"Tommen"));
	ASSERT_ZERO(list_insert(list2,111,"Myrcella"));
	ASSERT_ZERO(list_insert(list2,222,"Tywin"));

	ASSERT_ZERO(list_insert(list3,1111,"Jorah"));

	ASSERT_ZERO(list_compute(list1,11,youComputeNothing,&result));
	ASSERT_TEST(result == 2);
	ASSERT_ZERO(list_compute(list1,33,youComputeNothing,&result));
	ASSERT_TEST(result == 1);
	ASSERT_ZERO(list_compute(list1,66,youComputeNothing,&result));
	ASSERT_TEST(result == 1);
	ASSERT_ZERO(list_compute(list2,222,youComputeNothing,&result));
	ASSERT_TEST(result == 1);
	ASSERT_ZERO(list_compute(list2,444,youComputeNothing,&result));
	ASSERT_TEST(result == 2);
	ASSERT_ZERO(list_compute(list3,1111,youComputeNothing,&result));
	ASSERT_TEST(result == 2);

	ASSERT_ZERO(list_update(list1,11,"Rickon One Direction"));
	ASSERT_ZERO(list_update(list1,33,"Robb Zombie"));
	ASSERT_ZERO(list_update(list1,66,"Jon know-nothing Snow"));
	ASSERT_ZERO(list_update(list2,222,"Ty-win-ston churchill"));
	ASSERT_ZERO(list_update(list2,444,"Joffrey I'll tell mother"));
	ASSERT_ZERO(list_update(list3,1111,"Jorah in the zone"));

	ASSERT_ZERO(list_compute(list1,11,youComputeNothing,&result));
	ASSERT_TEST(result == 8);
	ASSERT_ZERO(list_compute(list1,33,youComputeNothing,&result));
	ASSERT_TEST(result == 4);
	ASSERT_ZERO(list_compute(list1,66,youComputeNothing,&result));
	ASSERT_TEST(result == 5);
	ASSERT_ZERO(list_compute(list2,222,youComputeNothing,&result));
	ASSERT_TEST(result == 4);
	ASSERT_ZERO(list_compute(list2,444,youComputeNothing,&result));
	ASSERT_TEST(result == 6);
	ASSERT_ZERO(list_compute(list3,1111,youComputeNothing,&result));
	ASSERT_TEST(result == 6);

	list_free(list1);
	list_free(list2);
	list_free(list3);
	return true;
}


int main(){
	RUN_TEST(testFreeErrors);
	RUN_TEST(testSplitErrors);
	RUN_TEST(testInsertErrors);
	RUN_TEST(testRemoveErrors);
	RUN_TEST(testFindErrors);
	RUN_TEST(testSizeErrors);
	RUN_TEST(testUpdateErrors);
	RUN_TEST(testComputeErrors);
	RUN_TEST(testBatchErrors);
	RUN_TEST(testSequential1);
	RUN_TEST(testSequential2);

	return 0;
}
