/*
 * stacked-area-c: stacked area charts (REAS) from CSV, rendered to SVG.
 *
 * Public API. See README.md for usage.
 */
#ifndef STACKED_AREA_H
#define STACKED_AREA_H

#include <stddef.h>
#include <stdio.h>

#define SA_MAX_SERIES_NAME 256

/*
 * A single data series: name + arrays of x/y values.
 *
 * x[i] and y[i] are defined for i in [0, n). y[i] == NAN marks a gap.
 */
typedef struct {
    char name[SA_MAX_SERIES_NAME];
    size_t n;
    double *x; /* always allocated by sa_dataset_add */
    double *y;
} sa_series;

typedef struct {
    sa_series *items;
    size_t count;
} sa_dataset;

/* Lifecycle. */
void sa_dataset_init(sa_dataset *ds);
void sa_dataset_free(sa_dataset *ds);

/*
 * Append a copy of `n` points. `x` may be NULL (then x[i] = i).
 * Returns 0 on success, -1 on allocation failure.
 */
int sa_dataset_add(sa_dataset *ds, const char *name,
                   const double *x, const double *y, size_t n);

/*
 * Load series from a CSV file:
 *   - line 1: header names (if the first token is "x", column 0 is the
 *     x axis; otherwise x defaults to the row index),
 *   - lines 2+: data rows (missing/short tokens treated as NAN gaps).
 * Returns 0 on success, -1 on error (missing file, malformed data).
 */
int sa_dataset_load_csv(sa_dataset *ds, const char *path);

/*
 * Stacked band for one series: y_low/y_high[i] give the band boundaries.
 */
typedef struct {
    const sa_series *base; /* series this band belongs to (points into ds) */
    size_t n;
    double *y_low;
    double *y_high;
} sa_band;

/*
 * Compute stacked bands for every series of the dataset (call
 * sa_bands_free to release). Returns 0 on success, -1 on allocation failure.
 */
int sa_stack_compute(const sa_dataset *ds,
                     sa_band **bands_out, size_t *bands_count_out);
void sa_bands_free(sa_band *bands, size_t count);

/* SVG rendering options. */
typedef struct {
    const char *title;          /* optional chart title */
    const char *x_label;        /* optional x axis label */
    const char *y_label;        /* optional y axis label */
    const char *const *colors;  /* NULL -> built-in palette */
    size_t n_colors;            /* ignored when colors == NULL */
    int hide_legend;            /* 0 (default) -> legend drawn */
    int flip_y;                 /* 0 (default) -> y grows upward on screen */
    double x_min, x_max;        /* 0,0 -> auto (from data) */
    double y_min, y_max;        /* 0,0 -> auto (from data) */
    double width, height;       /* 0,0 -> 640x420 */
    double left, top, right, bottom; /* 0 -> defaults (60, 20, 20, 40) */
} sa_svg_opts;

/*
 * Render the dataset as stacked areas (SVG) to `out`.
 * Returns 0 on success, -1 on error.
 */
int sa_render_svg(const sa_dataset *ds, const sa_band *bands,
                  size_t bands_count, const sa_svg_opts *opts, FILE *out);

#endif /* STACKED_AREA_H */
