/* $Id$ $Revision$ */
/* vim:set shiftwidth=4 ts=8: */

/**********************************************************
*      This software is part of the graphviz package      *
*                http://www.graphviz.org/                 *
*                                                         *
*            Copyright (c) 1994-2004 AT&T Corp.           *
*                and is licensed under the                *
*            Common Public License, Version 1.0           *
*                      by AT&T Corp.                      *
*                                                         *
*        Information and Software Systems Research        *
*              AT&T Research, Florham Park NJ             *
**********************************************************/


#include "bfs.h"
#include "dijkstra.h"
#include "kkutils.h"
#include <stdlib.h>
#include <math.h>

int common_neighbors(vtx_data * graph, int v, int u, int *v_vector)
{
    /* count number of common neighbors of 'v' and 'u' */
    int neighbor;
    int num_shared_neighbors = 0;
    int j;
    for (j = 1; j < graph[u].nedges; j++) {
	neighbor = graph[u].edges[j];
	if (v_vector[neighbor] > 0) {
	    /* a shared neighobr */
	    num_shared_neighbors++;
	}
    }
    return num_shared_neighbors;
}
void fill_neighbors_vec_unweighted(vtx_data * graph, int vtx, int *vtx_vec)
{
    /* a node is NOT a neighbor of itself! */
    /* unlike the other version of this function */
    int j;
    for (j = 1; j < graph[vtx].nedges; j++) {
	vtx_vec[graph[vtx].edges[j]] = 1;
    }
}

void empty_neighbors_vec(vtx_data * graph, int vtx, int *vtx_vec)
{
    int j;
    /* a node is NOT a neighbor of itself! */
    /* unlike the other version ofthis function */
    for (j = 1; j < graph[vtx].nedges; j++) {
	vtx_vec[graph[vtx].edges[j]] = 0;
    }
}

/* compute_apsp_dijkstra:
 * Assumes the graph has weights
 */
static DistType **compute_apsp_dijkstra(vtx_data * graph, int n)
{
    int i;
    DistType *storage;
    DistType **dij;

    storage = N_GNEW(n * n, DistType);
    dij = N_GNEW(n, DistType *);
    for (i = 0; i < n; i++)
	dij[i] = storage + i * n;

    for (i = 0; i < n; i++) {
	dijkstra(i, graph, n, dij[i]);
    }
    return dij;
}

static DistType **compute_apsp_simple(vtx_data * graph, int n)
{
    /* compute all pairs shortest path */
    /* for unweighted graph */
    int i;
    DistType *storage = N_GNEW(n * n, int);
    DistType **dij;
    Queue Q;

    dij = N_GNEW(n, DistType *);
    for (i = 0; i < n; i++) {
	dij[i] = storage + i * n;
    }
    mkQueue(&Q, n);
    for (i = 0; i < n; i++) {
	bfs(i, graph, n, dij[i], &Q);
    }
    freeQueue(&Q);
    return dij;
}

DistType **compute_apsp(vtx_data * graph, int n)
{
    if (graph->ewgts)
	return compute_apsp_dijkstra(graph, n);
    else
	return compute_apsp_simple(graph, n);
}

DistType **compute_apsp_artifical_weights(vtx_data * graph, int n)
{
    DistType **Dij;
    /* compute all-pairs-shortest-path-length while weighting the graph */
    /* so high-degree nodes are distantly located */

    float *old_weights = graph[0].ewgts;

    compute_new_weights(graph, n);
    Dij = compute_apsp_dijkstra(graph, n);
    restore_old_weights(graph, n, old_weights);
    return Dij;
}


/**********************/
/*				      */
/*  Quick Sort        */
/*				      */
/**********************/

static void
split_by_place(double *place, int *nodes, int first, int last, int *middle)
{
    unsigned int splitter =
	rand() * ((unsigned) (last - first)) / RAND_MAX + (unsigned) first;
    int val;
    double place_val;
    int left = first + 1;
    int right = last;
    int temp;

    val = nodes[splitter];
    nodes[splitter] = nodes[first];
    nodes[first] = val;
    place_val = place[val];

    while (left < right) {
	while (left < right && place[nodes[left]] <= place_val)
	    left++;
	while (left < right && place[nodes[right]] >= place_val)
	    right--;
	if (left < right) {
	    temp = nodes[left];
	    nodes[left] = nodes[right];
	    nodes[right] = temp;
	    left++;
	    right--;		/* (1) */

	}
    }
    /* in this point either, left==right (meeting), or left=right+1 (because of (1)) */
    /* we have to decide to which part the meeting point (or left) belongs. */
    if (place[nodes[left]] > place_val)
	left = left - 1;	/* notice that always left>first, because of its initialization */
    *middle = left;
    nodes[first] = nodes[*middle];
    nodes[*middle] = val;
}

double distance_kD(double **coords, int dim, int i, int j)
{
    /* compute a k-D Euclidean distance between 'coords[*][i]' and 'coords[*][j]' */
    double sum = 0;
    int k;
    for (k = 0; k < dim; k++) {
	sum +=
	    (coords[k][i] - coords[k][j]) * (coords[k][i] - coords[k][j]);
    }
    return sqrt(sum);
}

void quicksort_place(double *place, int *ordering, int first, int last)
{
    if (first < last) {
	int middle;
#ifdef __cplusplus
	split_by_place(place, ordering, first, last, middle);
#else
	split_by_place(place, ordering, first, last, &middle);
#endif
	quicksort_place(place, ordering, first, middle - 1);
	quicksort_place(place, ordering, middle + 1, last);
    }
}

void compute_new_weights(vtx_data * graph, int n)
{
    /* Reweight graph so that high degree nodes will be separated */

    int i, j;
    int nedges = 0;
    float *weights;
    int *vtx_vec = N_GNEW(n, int);
    int deg_i, deg_j, neighbor;

    for (i = 0; i < n; i++) {
	nedges += graph[i].nedges;
    }
    weights = N_GNEW(nedges, float);

    for (i = 0; i < n; i++) {
	vtx_vec[i] = 0;
    }

    for (i = 0; i < n; i++) {
	graph[i].ewgts = weights;
	fill_neighbors_vec_unweighted(graph, i, vtx_vec);
	deg_i = graph[i].nedges - 1;
	for (j = 1; j <= deg_i; j++) {
	    neighbor = graph[i].edges[j];
	    deg_j = graph[neighbor].nedges - 1;
	    weights[j] =
		(float) (deg_i + deg_j -
			 2 * common_neighbors(graph, i, neighbor,
					      vtx_vec));
	}
	empty_neighbors_vec(graph, i, vtx_vec);
	weights += graph[i].nedges;
    }
    free(vtx_vec);
}

void restore_old_weights(vtx_data * graph, int n, float *old_weights)
{
    int i;
    free(graph[0].ewgts);
    graph[0].ewgts = NULL;
    if (old_weights != NULL) {
	for (i = 0; i < n; i++) {
	    graph[i].ewgts = old_weights;
	    old_weights += graph[i].nedges;
	}
    }
}
