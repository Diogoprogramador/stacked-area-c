/* Unit tests for sa_render_svg: renders to a temp file and checks the SVG
 * contains the expected structural elements (root, title, series names,
 * paths, close tag). */
#include "stacked_area.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0;

static void check_str(const char *haystack, const char *needle, const char *msg)
{
    if (!haystack || !needle || !strstr(haystack, needle)) {
        fprintf(stderr, "FAIL: %s (missing \"%s\")\n",
                msg, needle ? needle : "(null)");
        failures++;
    }
}

static char *read_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return NULL;
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    long sz = ftell(f);
    if (sz < 0) {
        fclose(f);
        return NULL;
    }
    rewind(f);
    char *buf = malloc((size_t)sz + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }
    size_t got = fread(buf, 1, (size_t)sz, f);
    buf[got] = '\0';
    fclose(f);
    return buf;
}

int main(void)
{
    sa_dataset ds;
    sa_dataset_init(&ds);
    double y1[4] = {1.0, 2.0, 3.0, 4.0};
    double y2[4] = {0.5, 1.0, 1.5, 2.0};
    check_str(sa_dataset_add(&ds, "alpha", NULL, y1, 4) == 0 ? "ok" : "err",
              "ok", "add alpha");
    check_str(sa_dataset_add(&ds, "beta", NULL, y2, 4) == 0 ? "ok" : "err",
              "ok", "add beta");

    sa_band *bands = NULL;
    size_t nb = 0;
    check_str(sa_stack_compute(&ds, &bands, &nb) == 0 ? "ok" : "err",
              "ok", "stack compute");

    const char *path = "sa_test_svg_out.svg";
    FILE *f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "FAIL: cannot open %s for writing\n", path);
        failures++;
    } else {
        sa_svg_opts opts;
        memset(&opts, 0, sizeof opts);
        opts.title = "Demo";

        int rc = sa_render_svg(&ds, bands, nb, &opts, f);
        fclose(f);
        if (rc != 0) {
            fprintf(stderr, "FAIL: sa_render_svg returned %d\n", rc);
            failures++;
        }

        char *out = read_file(path);
        check_str(out, "<?xml", "xml declaration");
        check_str(out, "<svg", "svg root element");
        check_str(out, "</svg>", "svg close tag");
        check_str(out, "Demo", "title text");
        check_str(out, "alpha", "legend contains series name alpha");
        check_str(out, "beta", "legend contains series name beta");
        check_str(out, "<path", "area path element");
        check_str(out, "fill", "fill attribute present");
        free(out);
        remove(path);
    }

    sa_bands_free(bands, nb);
    sa_dataset_free(&ds);

    if (failures) {
        fprintf(stderr, "%d failure(s)\n", failures);
        return 1;
    }
    printf("test_svg: OK\n");
    return 0;
}
