#include <stdlib.h>
#include <pthread.h>
#include <stdio.h>
#include "genetic_algorithm_par.h"

int main(int argc, char *argv[]) {
	// array with all the objects that can be placed in the sack
	sack_object *objects = NULL;
	// number of objects
	int object_count = 0;
	// maximum weight that can be carried in the sack
	int sack_capacity = 0;
	// number of generations
	int generations_count = 0;
	// number of threads
	int P = 0;

	if (!read_input(&objects, &object_count, &sack_capacity, &generations_count, &P, argc, argv)) {
		return 0;
	}


	int r;
	pthread_barrier_t barrier;
	status args[P];
	pthread_t threads[P];
	r = pthread_barrier_init(&barrier, NULL, P);
	if(r != 0){
		printf("\n Barrier init failed\n");
		return -1;
	}
	current_generation = (individual*) calloc(object_count, sizeof(individual));
	next_generation = (individual*) calloc(object_count, sizeof(individual));
	tmp = NULL;

	for(int i =0; i < P ; i++){
		args[i].id = i;
		args[i].objects = objects;
		args[i].generations_count = generations_count;
		args[i].sack_capacity = sack_capacity;
		args[i].P = P;
		args[i].object_count = object_count;
		args[i].barrier = &barrier;
		r = pthread_create(&threads[i], NULL, run_genetic_algorithm_par, &args[i]);

		if (r) {
			printf("Eroare la crearea thread-ului %d\n", i);
			exit(-1);
		}
	}


	for (int id = 0; id < P; id++) {
		r = pthread_join(threads[id], NULL);

		if (r) {
			printf("Eroare la asteptarea thread-ului %d\n", id);
			exit(-1);
		}
  	}

	//run_genetic_algorithm_par(objects, object_count, generations_count, sack_capacity, P);

	free(objects);
	r = pthread_barrier_destroy(&barrier);
	if(r != 0){
		printf("Barrier destroy failed\n");
		return -1;
	}
	return 0;
}
