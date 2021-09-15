/*
 * Copyright (C) 2007 Vitor Sessak <vitor1001@gmail.com>
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

/**
 * @file
 * Codebook Generator using the ELBG algorithm
 */

#include <string.h>

#include "libavutil/avassert.h"
#include "libavutil/common.h"
#include "libavutil/lfg.h"
#include "elbg.h"

#define DELTA_ERR_MAX 0.1  ///< Precision of the ELBG algorithm (as percentage error)

/**
 * In the ELBG jargon, a cell is the set of points that are closest to a
 * codebook entry. Not to be confused with a RoQ Video cell. */
typedef struct cell_s {
    int index;
    struct cell_s *next;
} cell;

/**
 * ELBG internal data
 */
typedef struct ELBGContext {
    int64_t error;
    int dim;
    int num_cb;
    int *codebook;
    cell **cells;
    int64_t *utility;
    int64_t *utility_inc;
    int *nearest_cb;
    int *points;
    AVLFG *rand_state;
    int *scratchbuf;
} ELBGContext;

static inline int distance_limited(int *a, int *b, int dim, int limit)
{
    int i, dist=0;
    for (i=0; i<dim; i++) {
        dist += (a[i] - b[i])*(a[i] - b[i]);
        if (dist > limit)
            return INT_MAX;
    }

    return dist;
}

static inline void vect_division(int *res, int *vect, int div, int dim)
{
    int i;
    if (div > 1)
        for (i=0; i<dim; i++)
            res[i] = ROUNDED_DIV(vect[i],div);
    else if (res != vect)
        memcpy(res, vect, dim*sizeof(int));

}

static int eval_error_cell(ELBGContext *elbg, int *centroid, cell *cells)
{
    int error=0;
    for (; cells; cells=cells->next)
        error += distance_limited(centroid, elbg->points + cells->index*elbg->dim, elbg->dim, INT_MAX);

    return error;
}

static int get_closest_codebook(ELBGContext *elbg, int index)
{
    int pick = 0;
    for (int i = 0, diff_min = INT_MAX; i < elbg->num_cb; i++)
        if (i != index) {
            int diff;
            diff = distance_limited(elbg->codebook + i*elbg->dim, elbg->codebook + index*elbg->dim, elbg->dim, diff_min);
            if (diff < diff_min) {
                pick = i;
                diff_min = diff;
            }
        }
    return pick;
}

static int get_high_utility_cell(ELBGContext *elbg)
{
    int i=0;
    /* Using linear search, do binary if it ever turns to be speed critical */
    uint64_t r;

    if (elbg->utility_inc[elbg->num_cb - 1] < INT_MAX) {
        r = av_lfg_get(elbg->rand_state) % (unsigned int)elbg->utility_inc[elbg->num_cb - 1] + 1;
    } else {
        r = av_lfg_get(elbg->rand_state);
        r = (av_lfg_get(elbg->rand_state) + (r<<32)) % elbg->utility_inc[elbg->num_cb - 1] + 1;
    }

    while (elbg->utility_inc[i] < r) {
        i++;
    }

    av_assert2(elbg->cells[i]);

    return i;
}

/**
 * Implementation of the simple LBG algorithm for just two codebooks
 */
static int simple_lbg(ELBGContext *elbg,
                      int dim,
                      int *centroid[3],
                      int newutility[3],
                      int *points,
                      cell *cells)
{
    int i, idx;
    int numpoints[2] = {0,0};
    int *newcentroid[2] = {
        elbg->scratchbuf + 3*dim,
        elbg->scratchbuf + 4*dim
    };
    cell *tempcell;

    memset(newcentroid[0], 0, 2 * dim * sizeof(*newcentroid[0]));

    newutility[0] =
    newutility[1] = 0;

    for (tempcell = cells; tempcell; tempcell=tempcell->next) {
        idx = distance_limited(centroid[0], points + tempcell->index*dim, dim, INT_MAX)>=
              distance_limited(centroid[1], points + tempcell->index*dim, dim, INT_MAX);
        numpoints[idx]++;
        for (i=0; i<dim; i++)
            newcentroid[idx][i] += points[tempcell->index*dim + i];
    }

    vect_division(centroid[0], newcentroid[0], numpoints[0], dim);
    vect_division(centroid[1], newcentroid[1], numpoints[1], dim);

    for (tempcell = cells; tempcell; tempcell=tempcell->next) {
        int dist[2] = {distance_limited(centroid[0], points + tempcell->index*dim, dim, INT_MAX),
                       distance_limited(centroid[1], points + tempcell->index*dim, dim, INT_MAX)};
        int idx = dist[0] > dist[1];
        newutility[idx] += dist[idx];
    }

    return newutility[0] + newutility[1];
}

static void get_new_centroids(ELBGContext *elbg, int huc, int *newcentroid_i,
                              int *newcentroid_p)
{
    cell *tempcell;
    int *min = newcentroid_i;
    int *max = newcentroid_p;
    int i;

    for (i=0; i< elbg->dim; i++) {
        min[i]=INT_MAX;
        max[i]=0;
    }

    for (tempcell = elbg->cells[huc]; tempcell; tempcell = tempcell->next)
        for(i=0; i<elbg->dim; i++) {
            min[i]=FFMIN(min[i], elbg->points[tempcell->index*elbg->dim + i]);
            max[i]=FFMAX(max[i], elbg->points[tempcell->index*elbg->dim + i]);
        }

    for (i=0; i<elbg->dim; i++) {
        int ni = min[i] + (max[i] - min[i])/3;
        int np = min[i] + (2*(max[i] - min[i]))/3;
        newcentroid_i[i] = ni;
        newcentroid_p[i] = np;
    }
}

/**
 * Add the points in the low utility cell to its closest cell. Split the high
 * utility cell, putting the separated points in the (now empty) low utility
 * cell.
 *
 * @param elbg         Internal elbg data
 * @param indexes      {luc, huc, cluc}
 * @param newcentroid  A vector with the position of the new centroids
 */
static void shift_codebook(ELBGContext *elbg, int *indexes,
                           int *newcentroid[3])
{
    cell *tempdata;
    cell **pp = &elbg->cells[indexes[2]];

    while(*pp)
        pp= &(*pp)->next;

    *pp = elbg->cells[indexes[0]];

    elbg->cells[indexes[0]] = NULL;
    tempdata = elbg->cells[indexes[1]];
    elbg->cells[indexes[1]] = NULL;

    while(tempdata) {
        cell *tempcell2 = tempdata->next;
        int idx = distance_limited(elbg->points + tempdata->index*elbg->dim,
                           newcentroid[0], elbg->dim, INT_MAX) >
                  distance_limited(elbg->points + tempdata->index*elbg->dim,
                           newcentroid[1], elbg->dim, INT_MAX);

        tempdata->next = elbg->cells[indexes[idx]];
        elbg->cells[indexes[idx]] = tempdata;
        tempdata = tempcell2;
    }
}

static void evaluate_utility_inc(ELBGContext *elbg)
{
    int64_t inc=0;

    for (int i = 0; i < elbg->num_cb; i++) {
        if (elbg->num_cb * elbg->utility[i] > elbg->error)
            inc += elbg->utility[i];
        elbg->utility_inc[i] = inc;
    }
}


static void update_utility_and_n_cb(ELBGContext *elbg, int idx, int newutility)
{
    cell *tempcell;

    elbg->utility[idx] = newutility;
    for (tempcell=elbg->cells[idx]; tempcell; tempcell=tempcell->next)
        elbg->nearest_cb[tempcell->index] = idx;
}

/**
 * Evaluate if a shift lower the error. If it does, call shift_codebooks
 * and update elbg->error, elbg->utility and elbg->nearest_cb.
 *
 * @param elbg  Internal elbg data
 * @param idx   {luc (low utility cell, huc (high utility cell), cluc (closest cell to low utility cell)}
 */
static void try_shift_candidate(ELBGContext *elbg, int idx[3])
{
    int j, k, cont=0;
    int64_t olderror=0, newerror;
    int newutility[3];
    int *newcentroid[3] = {
        elbg->scratchbuf,
        elbg->scratchbuf + elbg->dim,
        elbg->scratchbuf + 2*elbg->dim
    };
    cell *tempcell;

    for (j=0; j<3; j++)
        olderror += elbg->utility[idx[j]];

    memset(newcentroid[2], 0, elbg->dim*sizeof(int));

    for (k=0; k<2; k++)
        for (tempcell=elbg->cells[idx[2*k]]; tempcell; tempcell=tempcell->next) {
            cont++;
            for (j=0; j<elbg->dim; j++)
                newcentroid[2][j] += elbg->points[tempcell->index*elbg->dim + j];
        }

    vect_division(newcentroid[2], newcentroid[2], cont, elbg->dim);

    get_new_centroids(elbg, idx[1], newcentroid[0], newcentroid[1]);

    newutility[2]  = eval_error_cell(elbg, newcentroid[2], elbg->cells[idx[0]]);
    newutility[2] += eval_error_cell(elbg, newcentroid[2], elbg->cells[idx[2]]);

    newerror = newutility[2];

    newerror += simple_lbg(elbg, elbg->dim, newcentroid, newutility, elbg->points,
                           elbg->cells[idx[1]]);

    if (olderror > newerror) {
        shift_codebook(elbg, idx, newcentroid);

        elbg->error += newerror - olderror;

        for (j=0; j<3; j++)
            update_utility_and_n_cb(elbg, idx[j], newutility[j]);

        evaluate_utility_inc(elbg);
    }
 }

/**
 * Implementation of the ELBG block
 */
static void do_shiftings(ELBGContext *elbg)
{
    int idx[3];

    evaluate_utility_inc(elbg);

    for (idx[0]=0; idx[0] < elbg->num_cb; idx[0]++)
        if (elbg->num_cb * elbg->utility[idx[0]] < elbg->error) {
            if (elbg->utility_inc[elbg->num_cb - 1] == 0)
                return;

            idx[1] = get_high_utility_cell(elbg);
            idx[2] = get_closest_codebook(elbg, idx[0]);

            if (idx[1] != idx[0] && idx[1] != idx[2])
                try_shift_candidate(elbg, idx);
        }
}

static int do_elbg(int *points, int dim, int numpoints, int *codebook,
                   int num_cb, int max_steps, int *closest_cb,
                AVLFG *rand_state)
{
    int dist;
    ELBGContext elbg_d;
    ELBGContext *elbg = &elbg_d;
    int i, j, steps = 0, ret = 0;
    int *size_part = av_malloc_array(num_cb, sizeof(int));
    cell *list_buffer = av_malloc_array(numpoints, sizeof(cell));
    cell *free_cells;
    int best_dist, best_idx = 0;
    int64_t last_error;

    elbg->error = INT64_MAX;
    elbg->dim = dim;
    elbg->num_cb = num_cb;
    elbg->codebook = codebook;
    elbg->cells = av_malloc_array(num_cb, sizeof(cell *));
    elbg->utility = av_malloc_array(num_cb, sizeof(*elbg->utility));
    elbg->nearest_cb = closest_cb;
    elbg->points = points;
    elbg->utility_inc = av_malloc_array(num_cb, sizeof(*elbg->utility_inc));
    elbg->scratchbuf = av_malloc_array(5*dim, sizeof(int));

    if (!size_part || !list_buffer || !elbg->cells ||
        !elbg->utility || !elbg->utility_inc || !elbg->scratchbuf) {
        ret = AVERROR(ENOMEM);
        goto out;
    }

    elbg->rand_state = rand_state;

    do {
        free_cells = list_buffer;
        last_error = elbg->error;
        steps++;
        memset(elbg->utility, 0, num_cb * sizeof(*elbg->utility));
        memset(elbg->cells,   0, num_cb * sizeof(*elbg->cells));

        elbg->error = 0;

        /* This loop evaluate the actual Voronoi partition. It is the most
           costly part of the algorithm. */
        for (i=0; i < numpoints; i++) {
            best_dist = distance_limited(elbg->points + i*elbg->dim, elbg->codebook + best_idx*elbg->dim, dim, INT_MAX);
            for (int k = 0; k < elbg->num_cb; k++) {
                dist = distance_limited(elbg->points + i*elbg->dim, elbg->codebook + k*elbg->dim, dim, best_dist);
                if (dist < best_dist) {
                    best_dist = dist;
                    best_idx = k;
                }
            }
            elbg->nearest_cb[i] = best_idx;
            elbg->error += best_dist;
            elbg->utility[elbg->nearest_cb[i]] += best_dist;
            free_cells->index = i;
            free_cells->next = elbg->cells[elbg->nearest_cb[i]];
            elbg->cells[elbg->nearest_cb[i]] = free_cells;
            free_cells++;
        }

        do_shiftings(elbg);

        memset(size_part, 0, num_cb * sizeof(*size_part));

        memset(elbg->codebook, 0, elbg->num_cb * dim * sizeof(*elbg->codebook));

        for (i=0; i < numpoints; i++) {
            size_part[elbg->nearest_cb[i]]++;
            for (j=0; j < elbg->dim; j++)
                elbg->codebook[elbg->nearest_cb[i]*elbg->dim + j] +=
                    elbg->points[i*elbg->dim + j];
        }

        for (int i = 0; i < elbg->num_cb; i++)
            vect_division(elbg->codebook + i*elbg->dim,
                          elbg->codebook + i*elbg->dim, size_part[i], elbg->dim);

    } while(((last_error - elbg->error) > DELTA_ERR_MAX*elbg->error) &&
            (steps < max_steps));

out:
    av_free(size_part);
    av_free(elbg->utility);
    av_free(list_buffer);
    av_free(elbg->cells);
    av_free(elbg->utility_inc);
    av_free(elbg->scratchbuf);
    return ret;
}

#define BIG_PRIME 433494437LL

/**
 * Initialize the codebook vector for the elbg algorithm.
 * If numpoints <= 24 * num_cb this function fills codebook with random numbers.
 * If not, it calls do_elbg for a (smaller) random sample of the points in
 * points.
 * @return < 0 in case of error, 0 otherwise
 */
static int init_elbg(int *points, int dim, int numpoints, int *codebook,
                     int num_cb, int max_steps, int *closest_cb,
                     AVLFG *rand_state)
{
    int ret = 0;

    if (numpoints > 24LL * num_cb) {
        /* ELBG is very costly for a big number of points. So if we have a lot
           of them, get a good initial codebook to save on iterations       */
        int *temp_points = av_malloc_array(dim, (numpoints/8)*sizeof(*temp_points));
        if (!temp_points)
            return AVERROR(ENOMEM);
        for (int i = 0; i < numpoints / 8; i++) {
            int k = (i*BIG_PRIME) % numpoints;
            memcpy(temp_points + i*dim, points + k*dim, dim * sizeof(*temp_points));
        }

        ret = init_elbg(temp_points, dim, numpoints / 8, codebook,
                        num_cb, 2 * max_steps, closest_cb, rand_state);
        if (ret < 0) {
            av_freep(&temp_points);
            return ret;
        }
        ret = do_elbg  (temp_points, dim, numpoints / 8, codebook,
                        num_cb, 2 * max_steps, closest_cb, rand_state);
        av_free(temp_points);
    } else  // If not, initialize the codebook with random positions
        for (int i = 0; i < num_cb; i++)
            memcpy(codebook + i * dim, points + ((i*BIG_PRIME)%numpoints)*dim,
                   dim * sizeof(*codebook));
    return ret;
}

int avpriv_elbg_do(ELBGContext **elbgp, int *points, int dim, int numpoints,
                   int *codebook, int num_cb, int max_steps,
                   int *closest_cb, AVLFG *rand_state)
{
    ELBGContext *const elbg = *elbgp ? *elbgp : av_mallocz(sizeof(*elbg));
    int ret;

    if (!elbg)
        return AVERROR(ENOMEM);
    *elbgp = elbg;

    ret = init_elbg(points, dim, numpoints, codebook,
                    num_cb, max_steps, closest_cb, rand_state);
    if (ret < 0)
        return ret;
    return do_elbg (points, dim, numpoints, codebook,
                    num_cb, max_steps, closest_cb, rand_state);
}

av_cold void avpriv_elbg_free(ELBGContext **elbgp)
{
    av_freep(elbgp);
}
