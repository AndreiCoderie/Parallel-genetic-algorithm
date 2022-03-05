#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "genetic_algorithm_par.h"
#include <pthread.h>
#include <math.h>


int read_input(sack_object **objects, int *object_count, int *sack_capacity, int *generations_count,  int *P, int argc, char *argv[])
{
	FILE *fp;

	if (argc < 3) {
		fprintf(stderr, "Usage:\n\t./tema1 in_file generations_count\n");
		return 0;
	}

	fp = fopen(argv[1], "r");
	if (fp == NULL) {
		return 0;
	}

	if (fscanf(fp, "%d %d", object_count, sack_capacity) < 2) {
		fclose(fp);
		return 0;
	}

	if (*object_count % 10) {
		fclose(fp);
		return 0;
	}

	sack_object *tmp_objects = (sack_object *) calloc(*object_count, sizeof(sack_object));

	for (int i = 0; i < *object_count; ++i) {
		if (fscanf(fp, "%d %d", &tmp_objects[i].profit, &tmp_objects[i].weight) < 2) {
			free(objects);
			fclose(fp);
			return 0;
		}
	}

	fclose(fp);

	*generations_count = (int) strtol(argv[2], NULL, 10);
	
	if (*generations_count == 0) {
		free(tmp_objects);

		return 0;
	}

	*objects = tmp_objects;

	*P = (int) strtol(argv[3], NULL, 10);

	return 1;
}

void print_objects(const sack_object *objects, int object_count)
{
	for (int i = 0; i < object_count; ++i) {
		printf("%d %d\n", objects[i].weight, objects[i].profit);
	}
}

void print_generation(const individual *generation, int limit)
{
	for (int i = 0; i < limit; ++i) {
		for (int j = 0; j < generation[i].chromosome_length; ++j) {
			printf("%d ", generation[i].chromosomes[j]);
		}

		printf("\n%d - %d\n", i, generation[i].fitness);
	}
}

void print_best_fitness(const individual *generation)
{
	printf("%d\n", generation[0].fitness);
}

void compute_fitness_function(const sack_object *objects, individual *generation, int object_count, int sack_capacity, int id, int P)
{
	int weight;
	int profit;
	int start = id * (double) object_count / P;
    int end = fmin((id + 1) * (double) object_count / P,object_count);

	for (int i = start; i < end; ++i) {
		weight = 0;
		profit = 0;
		generation[i].numbersOf1 = 0;

		for (int j = 0; j < generation[i].chromosome_length; ++j) {
			if (generation[i].chromosomes[j]) {
				weight += objects[j].weight;
				profit += objects[j].profit;
				generation[i].numbersOf1++;
			}
		}

		generation[i].fitness = (weight <= sack_capacity) ? profit : 0;
	}
}

int cmpfunc(const void *a, const void *b)
{
	individual *first = (individual *) a;
	individual *second = (individual *) b;

	int res = second->fitness - first->fitness; // decreasing by fitness
	if (res == 0) {
		res = first->numbersOf1 - second->numbersOf1; // increasing by number of objects in the sack
		if (res == 0) {
			return second->index - first->index;
		}
	}

	return res;
}

void mutate_bit_string_1(const individual *ind, int generation_index)
{
	int i, mutation_size;
	int step = 1 + generation_index % (ind->chromosome_length - 2);

	if (ind->index % 2 == 0) {
		// for even-indexed individuals, mutate the first 40% chromosomes by a given step
		mutation_size = ind->chromosome_length * 4 / 10;
		for (i = 0; i < mutation_size; i += step) {
			ind->chromosomes[i] = 1 - ind->chromosomes[i];
		}
	} else {
		// for even-indexed individuals, mutate the last 80% chromosomes by a given step
		mutation_size = ind->chromosome_length * 8 / 10;
		for (i = ind->chromosome_length - mutation_size; i < ind->chromosome_length; i += step) {
			ind->chromosomes[i] = 1 - ind->chromosomes[i];
		}
	}
}

void mutate_bit_string_2(const individual *ind, int generation_index)
{
	int step = 1 + generation_index % (ind->chromosome_length - 2);

	// mutate all chromosomes by a given step
	for (int i = 0; i < ind->chromosome_length; i += step) {
		ind->chromosomes[i] = 1 - ind->chromosomes[i];
	}
}

void crossover(individual *parent1, individual *child1, int generation_index)
{
	individual *parent2 = parent1 + 1;
	individual *child2 = child1 + 1;
	int count = 1 + generation_index % parent1->chromosome_length;

	memcpy(child1->chromosomes, parent1->chromosomes, count * sizeof(int));
	memcpy(child1->chromosomes + count, parent2->chromosomes + count, (parent1->chromosome_length - count) * sizeof(int));

	memcpy(child2->chromosomes, parent2->chromosomes, count * sizeof(int));
	memcpy(child2->chromosomes + count, parent1->chromosomes + count, (parent1->chromosome_length - count) * sizeof(int));
}

void copy_individual(const individual *from, const individual *to)
{
	memcpy(to->chromosomes, from->chromosomes, from->chromosome_length * sizeof(int));
}

void free_generation(individual *generation)
{
	int i;

	for (i = 0; i < generation->chromosome_length; ++i) {
		free(generation[i].chromosomes);
		generation[i].chromosomes = NULL;
		generation[i].fitness = 0;
	}
}

void initialize(status elements){
	int start = elements.id * (double) elements.object_count / elements.P;
    int end = fmin((elements.id + 1) * (double) elements.object_count / elements.P, elements.object_count);
	for (int i = start; i < end; ++i) {
		current_generation[i].fitness = 0;
		current_generation[i].chromosomes = (int*) calloc(elements.object_count, sizeof(int));
		current_generation[i].chromosomes[i] = 1;
		current_generation[i].index = i;
		current_generation[i].chromosome_length = elements.object_count;

		next_generation[i].fitness = 0;
		next_generation[i].chromosomes = (int*) calloc(elements.object_count, sizeof(int));
		next_generation[i].index = i;
		next_generation[i].chromosome_length = elements.object_count;
	}
	pthread_barrier_wait(elements.barrier);

}

void copy_ind(status elements, int count, int cursor){
	// keep first 30% children (elite children selection)
	int start = elements.id * (double) count / elements.P;
	int end = fmin((elements.id + 1) * (double) count / elements.P, count);
	for (int i = start; i < end; ++i) {
		copy_individual(current_generation + i, next_generation + i);
	}
	pthread_barrier_wait(elements.barrier);

}

void mutate1(status elements,int cursor, int count, int k){
	int start = elements.id * (double) count / elements.P;
	int end = fmin((elements.id + 1) * (double) count / elements.P, count);
	for (int i = start; i < end; ++i) {
		copy_individual(current_generation + i, next_generation + cursor + i);
		mutate_bit_string_1(next_generation + cursor + i, k);
	}	
	pthread_barrier_wait(elements.barrier);

}

void mutate2(status elements, int cursor, int count, int k){
	int start = elements.id * (double) count / elements.P;
	int end = fmin((elements.id + 1) * (double) count / elements.P, count);
	for (int i = start; i < end; ++i) {
		copy_individual(current_generation + i + count, next_generation + cursor + i);
		mutate_bit_string_2(next_generation + cursor + i, k);
	}
	pthread_barrier_wait(elements.barrier);

}

void* run_genetic_algorithm_par(void *arg)
{	

	int count, cursor;
	status elements = *(status*) arg; 
	// current_generation = (individual*) calloc(elements.object_count, sizeof(individual));
	// next_generation = (individual*) calloc(elements.object_count, sizeof(individual));
	// tmp = NULL;

	//int start, end;

	initialize(elements);

	// iterate for each generation
	for (int k = 0; k < elements.generations_count; ++k) {
		cursor = 0;
		compute_fitness_function(elements.objects, current_generation, elements.object_count, elements.sack_capacity, elements.id, elements.P);
		//--------------------------------------
		pthread_barrier_wait(elements.barrier);
		if(elements.id == 0){
			qsort(current_generation, elements.object_count, sizeof(individual), cmpfunc);
		}
		pthread_barrier_wait(elements.barrier);



		count = elements.object_count * 3 / 10;
		copy_ind(elements, count, cursor);
		cursor = count;

		// mutate first 20% children with the first version of bit string mutation
		count = elements.object_count * 2 / 10;
		mutate1(elements, cursor, count, k);
		cursor += count;

		// mutate next 20% children with the second version of bit string mutation
		count = elements.object_count * 2 / 10;
		mutate2(elements, cursor, count, k);
		cursor += count;

		// crossover first 30% parents with one-point crossover
		// (if there is an odd number of parents, the last one is kept as such)
				
		count = elements.object_count * 3 / 10;

		if (count % 2 == 1) {
			copy_individual(current_generation + elements.object_count - 1, next_generation + cursor + count - 1);
			count--;
		}
			
		pthread_barrier_wait(elements.barrier);
			if(elements.id == 0){
				for (int i = 0; i < count; i += 2) {	
					crossover(current_generation + i, next_generation + cursor + i, k);
				}
			}
		pthread_barrier_wait(elements.barrier);

		// switch to new generation
		if(elements.id == 0){
			tmp = current_generation;
			
			current_generation = next_generation;
			next_generation = tmp;
		}	
		
		for (int i = 0; i < elements.object_count; ++i) {
			current_generation[i].index = i;
		}
		if(elements.id == 0){
			if (k % 5 == 0) {
				print_best_fitness(current_generation);
			}
		}


	}
	compute_fitness_function(elements.objects, current_generation, elements.object_count, elements.sack_capacity, elements.id, elements.P);
	pthread_barrier_wait(elements.barrier);

	if(elements.id == 0){
		qsort(current_generation, elements.object_count, sizeof(individual), cmpfunc);
		print_best_fitness(current_generation);

		// free resources for old generation
		free_generation(current_generation);
		free_generation(next_generation);

		// free resources
		free(current_generation);
		free(next_generation);
	}
	pthread_barrier_wait(elements.barrier);


	pthread_exit(NULL);
}