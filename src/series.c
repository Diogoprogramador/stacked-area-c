/*
 * series.c - sa_dataset implementation: series lifecycle and CSV loading.
 */
#include "stacked_area.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

void sa_dataset_init(sa_dataset *ds)
{
    if (!ds)
        return;
    ds->items = NULL;
    ds->count = 0;
}

void sa_dataset_free(sa_dataset *ds)
{
    if (!ds)
        return;
    for (size_t i = 0; i < ds->count; i++) {
        free(ds->items[i].x);
        free(ds->items[i].y);
    }
    free(ds->items);
    sa_dataset_init(ds);
}

static void copy_name(char *dst, size_t dstsz, const char *src)
{
    if (!src)
        src = "";
    size_t i = 0;
    while (src[i] && i + 1 < dstsz) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

int sa_dataset_add(sa_dataset *ds, const char *name,
                   const double *x, const double *y, size_t n)
{
    if (!ds)
        return -1;

    sa_series *items = realloc(ds->items, (ds->count + 1) * sizeof *items);
    if (!items)
        return -1;
    ds->items = items;

    sa_series *s = &ds->items[ds->count];
    memset(s, 0, sizeof *s);
    copy_name(s->name, sizeof s->name, name);
    s->n = n;
    if (n > 0) {
        s->x = malloc(n * sizeof(double));
        s->y = malloc(n * sizeof(double));
        if (!s->x || !s->y) {
            free(s->x);
            s->x = NULL;
            free(s->y);
            s->y = NULL;
            s->n = 0;
            return -1;
        }
        for (size_t i = 0; i < n; i++) {
            s->x[i] = x ? x[i] : (double)i;
            s->y[i] = y ? y[i] : 0.0;
        }
    }
    ds->count++;
    return 0;
}

/* Parse one trimmed token as a double; returns NAN on empty/unparseable. */
static double token_value(const char *s, size_t n)
{
    if (n >= 64)
        return NAN; /* absurdly long token -> gap */
    char buf[64];
    memcpy(buf, s, n);
    buf[n] = '\0';
    char *end = NULL;
    double v = strtod(buf, &end);
    if (end == buf)
        return NAN;
    return v;
}

/* Parse a line into `out` (cap doubles): comma-separated segments, trimmed;
 * empty segments become NAN gaps. Returns the token count, 0 for an empty
 * line, or -1 if there are more tokens than `cap`. */
static int parse_line(const char *line, double *out, size_t cap)
{
    size_t k = 0;
    const char *p = line;
    for (;;) {
        const char *end = p;
        while (*end && *end != ',')
            end++;
        /* trim spaces/newlines around the segment */
        const char *s = p, *e = end;
        while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n')
            s++;
        while (e > s && (e[-1] == ' ' || e[-1] == '\t' ||
                         e[-1] == '\r' || e[-1] == '\n'))
            e--;
        if (k >= cap)
            return -1;
        out[k++] = token_value(s, (size_t)(e - s));
        if (*end == '\0')
            break;
        p = end + 1;
    }
    return (int)k;
}

/* Parse the header line into trimmed names; returns the count. */
static int parse_header(const char *line, char names[][SA_MAX_SERIES_NAME],
                        size_t max)
{
    size_t k = 0;
    const char *p = line;
    for (;;) {
        const char *end = p;
        while (*end && *end != ',')
            end++;
        const char *s = p, *e = end;
        while (*s == ' ' || *s == '\t')
            s++;
        while (e > s && (e[-1] == ' ' || e[-1] == '\t' ||
                         e[-1] == '\r' || e[-1] == '\n'))
            e--;
        if (k >= max)
            return -1;
        size_t n = (size_t)(e - s);
        if (n >= SA_MAX_SERIES_NAME)
            n = SA_MAX_SERIES_NAME - 1;
        memcpy(names[k], s, n);
        names[k][n] = '\0';
        k++;
        if (*end == '\0')
            break;
        p = end + 1;
    }
    return (int)k;
}

int sa_dataset_load_csv(sa_dataset *ds, const char *path)
{
    if (!ds || !path)
        return -1;

    FILE *fp = fopen(path, "r");
    if (!fp)
        return -1;

    char line[4096];
    if (!fgets(line, sizeof line, fp)) {
        fclose(fp);
        return 0; /* empty file: nothing to load */
    }

    char names[256][SA_MAX_SERIES_NAME];
    int nh = parse_header(line, names, 256);
    if (nh < 0 || nh == 0) {
        fclose(fp);
        return 0;
    }

    size_t ncols = (size_t)nh;
    int has_x = (strcmp(names[0], "x") == 0);

    /* Read all rows into a growable grid. */
    size_t cap = 0, nrows = 0;
    double *grid = NULL;
    while (fgets(line, sizeof line, fp)) {
        double vals[256];
        int nv = parse_line(line, vals, 256);
        if (nv < 0 || (size_t)nv > ncols) {
            free(grid);
            fclose(fp);
            return -1;
        }
        if (nv == 0)
            continue; /* blank line */
        if (nrows == cap) {
            size_t newcap = cap ? cap * 2 : 16;
            double *ng = realloc(grid, newcap * ncols * sizeof(double));
            if (!ng) {
                free(grid);
                fclose(fp);
                return -1;
            }
            grid = ng;
            cap = newcap;
        }
        double *row = grid + nrows * ncols;
        for (size_t i = 0; i < ncols; i++)
            row[i] = (i < (size_t)nv) ? vals[i] : NAN;
        nrows++;
    }
    fclose(fp);

    /* Build one series per data column (skipping the x column). */
    size_t nseries = has_x ? ncols - 1 : ncols;
    for (size_t s = 0; s < nseries; s++) {
        size_t col = has_x ? s + 1 : s;
        double *x = malloc(nrows * sizeof(double));
        double *y = malloc(nrows * sizeof(double));
        if (!x || !y) {
            free(x);
            free(y);
            free(grid);
            return -1;
        }
        for (size_t i = 0; i < nrows; i++) {
            double *row = grid + i * ncols;
            y[i] = row[col];
            x[i] = has_x ? row[0] : (double)i;
        }
        if (sa_dataset_add(ds, names[col], x, y, nrows) != 0) {
            free(x);
            free(y);
            free(grid);
            return -1;
        }
        free(x);
        free(y);
    }
    free(grid);
    return 0;
}
