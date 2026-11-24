/* Unit tests for the data-side of the library: sa_dataset, CSV loading
 * and sa_stack_compute. */
#include "stacked_area.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0;

static void check(int cond, const char *msg)
{
    if (!cond) {
        fprintf(stderr, "FAIL: %s\n", msg);
        failures++;
    }
}

static void check_eq(double a, double b, const char *msg)
{
    if (fabs(a - b) > 1e-9) {
        fprintf(stderr, "FAIL: %s (got %f, want %f)\n", msg, a, b);
        failures++;
    }
}

static int write_csv(const char *path, const char *text)
{
    FILE *f = fopen(path, "w");
    if (!f)
        return -1;
    fputs(text, f);
    return fclose(f) == 0 ? 0 : -1;
}

int main(void)
{
    /* --- sa_dataset_add + sa_stack_compute: stacking arithmetic --- */
    sa_dataset ds;
    sa_dataset_init(&ds);

    double y1[3] = {1.0, 2.0, 3.0};
    double y2[3] = {0.5, 1.5, 2.5};
    double y3[3] = {10.0, 20.0, 30.0};

    check(sa_dataset_add(&ds, "a", NULL, y1, 3) == 0, "add a");
    check(sa_dataset_add(&ds, "b", NULL, y2, 3) == 0, "add b");
    check(sa_dataset_add(&ds, "c", NULL, y3, 3) == 0, "add c");
    check(ds.count == 3, "3 series added");
    check_eq(ds.items[0].x[2], 2.0, "auto x = index");
    check_eq(ds.items[0].y[1], 2.0, "y copied");

    sa_band *bands = NULL;
    size_t nb = 0;
    check(sa_stack_compute(&ds, &bands, &nb) == 0, "stack compute");
    check(nb == 3, "3 bands");
    if (bands) {
        /* i=0: a -> [0,1]; b -> [1,1.5]; c -> [1.5,11.5] */
        check_eq(bands[0].y_low[0], 0.0, "a low[0] == 0");
        check_eq(bands[0].y_high[0], 1.0, "a high[0] == 1");
        check_eq(bands[1].y_low[0], 1.0, "b low[0] == 1");
        check_eq(bands[1].y_high[0], 1.5, "b high[0] == 1.5");
        check_eq(bands[2].y_low[0], 1.5, "c low[0] == 1.5");
        check_eq(bands[2].y_high[0], 11.5, "c high[0] == 11.5");
        /* i=2: run = 3; b -> [3,5.5]; c -> [5.5,35.5] */
        check_eq(bands[0].y_high[2], 3.0, "a high[2] == 3");
        check_eq(bands[1].y_low[2], 3.0, "b low[2] == 3");
        check_eq(bands[2].y_high[2], 35.5, "c high[2] == 35.5");
        check(bands[2].n == 3, "band n == 3");
        sa_bands_free(bands, nb);
    }

    /* --- gap handling: NaN -> zero-height band at the current level --- */
    {
        sa_dataset g;
        sa_dataset_init(&g);
        double a[3] = {1.0, 2.0, 3.0};
        double b[3] = {1.0, NAN, 1.0};
        check(sa_dataset_add(&g, "a", NULL, a, 3) == 0, "gap add a");
        check(sa_dataset_add(&g, "b", NULL, b, 3) == 0, "gap add b");

        sa_band *gb = NULL;
        size_t gn = 0;
        check(sa_stack_compute(&g, &gb, &gn) == 0, "stack with gap");
        if (gb && gn == 2) {
            check_eq(gb[0].y_high[0], 1.0, "a high[0] == 1");
            check_eq(gb[1].y_low[0], 1.0, "b low[0] == 1");
            check_eq(gb[1].y_high[0], 2.0, "b high[0] == 2");
            /* i=1: b is NaN -> low == high == 2 */
            check_eq(gb[1].y_low[1], 2.0, "b low[1] == 2 (gap)");
            check_eq(gb[1].y_high[1], 2.0, "b high[1] == 2 (gap)");
            /* i=2: b[2] = 1 -> low == 2, high == 3 */
            check_eq(gb[1].y_low[2], 3.0, "b low[2] == 3");
            check_eq(gb[1].y_high[2], 4.0, "b high[2] == 4");
            sa_bands_free(gb, gn);
        }
        sa_dataset_free(&g);
    }

    /* --- CSV loading: header + x column --- */
    {
        const char *path = "sa_test_stacked.csv";
        check(write_csv(path,
                        "x,s1,s2\n"
                        "0,1,10\n"
                        "1,2,20\n"
                        "2,3,30\n") == 0, "write csv");
        sa_dataset cds;
        sa_dataset_init(&cds);
        check(sa_dataset_load_csv(&cds, path) == 0, "load csv");
        check(cds.count == 2, "csv: 2 series");
        if (cds.count == 2) {
            check(strcmp(cds.items[0].name, "s1") == 0, "csv: name s1");
            check(cds.items[0].n == 3, "csv: n == 3");
            check_eq(cds.items[0].x[2], 2.0, "csv: x[2] == 2");
            check_eq(cds.items[0].y[1], 2.0, "csv: s1 y[1] == 2");
            check_eq(cds.items[1].y[2], 30.0, "csv: s2 y[2] == 30");
        }
        sa_dataset_free(&cds);
        remove(path);
    }

    /* --- CSV loading: no x column -> x = row index --- */
    {
        const char *path = "sa_test_stacked2.csv";
        check(write_csv(path,
                        "s1,s2\n"
                        "1,10\n"
                        "2,20\n") == 0, "write csv 2");
        sa_dataset cds;
        sa_dataset_init(&cds);
        check(sa_dataset_load_csv(&cds, path) == 0, "load csv 2");
        check(cds.count == 2, "csv2: 2 series");
        if (cds.count == 2) {
            check_eq(cds.items[0].x[0], 0.0, "csv2: x[0] == 0");
            check_eq(cds.items[0].x[1], 1.0, "csv2: x[1] == 1");
            check_eq(cds.items[1].y[0], 10.0, "csv2: s2 y[0] == 10");
        }
        sa_dataset_free(&cds);
        remove(path);
    }

    /* --- CSV loading: missing file is an error --- */
    {
        sa_dataset cds;
        sa_dataset_init(&cds);
        check(sa_dataset_load_csv(&cds, "no_such_file_sa_test.csv") != 0,
              "load missing file fails");
        sa_dataset_free(&cds);
    }

    sa_dataset_free(&ds);

    if (failures) {
        fprintf(stderr, "%d failure(s)\n", failures);
        return 1;
    }
    printf("test_stacked: OK\n");
    return 0;
}
