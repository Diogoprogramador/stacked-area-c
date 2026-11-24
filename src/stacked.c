/*
 * stacked.c - sa_stack_compute: computes the stacked band (y_low/y_high)
 * for each series.
 */
#include "stacked_area.h"

#include <math.h>
#include <stdlib.h>

int sa_stack_compute(const sa_dataset *ds,
                     sa_band **bands_out, size_t *bands_count_out)
{
    if (!ds || !bands_out || !bands_count_out)
        return -1;
    *bands_out = NULL;
    *bands_count_out = 0;
    if (ds->count == 0)
        return 0;

    /* Points per band: the longest series. */
    size_t n = 0;
    for (size_t s = 0; s < ds->count; s++)
        if (ds->items[s].n > n)
            n = ds->items[s].n;

    sa_band *bands = calloc(ds->count, sizeof *bands);
    if (!bands)
        return -1;
    for (size_t s = 0; s < ds->count; s++) {
        bands[s].base = &ds->items[s];
        bands[s].n = n;
        if (n > 0) {
            bands[s].y_low = calloc(n, sizeof(double));
            bands[s].y_high = calloc(n, sizeof(double));
            if (!bands[s].y_low || !bands[s].y_high) {
                sa_bands_free(bands, ds->count);
                return -1;
            }
        }
    }

    /* Stack series in order: run is the cumulative total of all series
     * up to and including the current one at each point. A NaN (gap)
     * contributes nothing, giving a zero-height band at the current level. */
    for (size_t i = 0; i < n; i++) {
        double run = 0.0;
        for (size_t s = 0; s < ds->count; s++) {
            const sa_series *ser = &ds->items[s];
            double y = (i < ser->n) ? ser->y[i] : NAN;
            double low = run;
            if (!isnan(y))
                run += y;
            bands[s].y_low[i] = low;
            bands[s].y_high[i] = run;
        }
    }

    *bands_out = bands;
    *bands_count_out = ds->count;
    return 0;
}

void sa_bands_free(sa_band *bands, size_t count)
{
    if (!bands)
        return;
    for (size_t i = 0; i < count; i++) {
        free(bands[i].y_low);
        free(bands[i].y_high);
    }
    free(bands);
}
