/*
 * sa-cli: command-line interface for stacked-area-c.
 *
 * Usage: sa-cli render FILE [-o OUT.SVG] [options]
 */
#include "stacked_area.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(FILE *out, const char *prog)
{
    fprintf(out,
            "Usage:\n"
            "  %s render FILE [-o OUT.SVG] [options]\n"
            "  %s --help\n"
            "\n"
            "Render a stacked area chart (REAS) from a CSV file.\n"
            "\n"
            "CSV format: the first line is the header. If the first column is\n"
            "\"x\", it is used as the x axis; otherwise x defaults to the row\n"
            "index. Remaining columns are the series, in order.\n"
            "\n"
            "Options:\n"
            "  -o, --output FILE   write the SVG to FILE (default: stdout)\n"
            "  -t, --title STR     chart title\n"
            "  -x, --x-label STR   x axis label\n"
            "  -y, --y-label STR   y axis label\n"
            "      --no-legend     hide the legend\n"
            "  -h, --help          show this help\n",
            prog, prog);
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        usage(stderr, argv[0]);
        return 2;
    }
    if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        usage(stdout, argv[0]);
        return 0;
    }
    if (strcmp(argv[1], "render") != 0) {
        fprintf(stderr, "error: unknown command '%s'\n\n", argv[1]);
        usage(stderr, argv[0]);
        return 2;
    }

    const char *input = NULL;
    const char *output = NULL;
    const char *title = NULL;
    const char *xlabel = NULL;
    const char *ylabel = NULL;
    int hide_legend = 0;

    int i = 2;
    while (i < argc) {
        const char *a = argv[i];
        const char *v;
        int needs_val;
        if (strcmp(a, "-o") == 0 || strcmp(a, "--output") == 0) {
            needs_val = 1;
        } else if (strcmp(a, "-t") == 0 || strcmp(a, "--title") == 0) {
            needs_val = 1;
        } else if (strcmp(a, "-x") == 0 || strcmp(a, "--x-label") == 0) {
            needs_val = 1;
        } else if (strcmp(a, "-y") == 0 || strcmp(a, "--y-label") == 0) {
            needs_val = 1;
        } else if (strcmp(a, "--no-legend") == 0) {
            needs_val = 0;
        } else if (strcmp(a, "-h") == 0 || strcmp(a, "--help") == 0) {
            usage(stdout, argv[0]);
            return 0;
        } else if (a[0] == '-' && a[1] != '\0') {
            fprintf(stderr, "error: unknown option '%s'\n\n", a);
            usage(stderr, argv[0]);
            return 2;
        } else if (input) {
            fprintf(stderr, "error: unexpected argument '%s'\n\n", a);
            usage(stderr, argv[0]);
            return 2;
        } else {
            input = a;
            i++;
            continue;
        }

        if (needs_val) {
            if (++i >= argc) {
                fprintf(stderr, "error: missing value for '%s'\n\n", a);
                usage(stderr, argv[0]);
                return 2;
            }
            v = argv[i];
        } else {
            v = "1";
        }

        if (strcmp(a, "-o") == 0 || strcmp(a, "--output") == 0) {
            output = v;
        } else if (strcmp(a, "-t") == 0 || strcmp(a, "--title") == 0) {
            title = v;
        } else if (strcmp(a, "-x") == 0 || strcmp(a, "--x-label") == 0) {
            xlabel = v;
        } else {
            ylabel = v; /* -y / --y-label */
        }
        i++;
    }

    if (!input) {
        fprintf(stderr, "error: missing input file\n\n");
        usage(stderr, argv[0]);
        return 2;
    }

    sa_dataset ds;
    sa_dataset_init(&ds);
    if (sa_dataset_load_csv(&ds, input) != 0) {
        fprintf(stderr, "error: cannot load '%s'\n", input);
        sa_dataset_free(&ds);
        return 1;
    }

    sa_band *bands = NULL;
    size_t nb = 0;
    if (sa_stack_compute(&ds, &bands, &nb) != 0) {
        fprintf(stderr, "error: out of memory\n");
        sa_dataset_free(&ds);
        return 1;
    }

    sa_svg_opts opts;
    memset(&opts, 0, sizeof opts);
    opts.title = title;
    opts.x_label = xlabel;
    opts.y_label = ylabel;
    opts.hide_legend = hide_legend;

    FILE *out = stdout;
    int close_out = 0;
    if (output) {
        out = fopen(output, "w");
        if (!out) {
            fprintf(stderr, "error: cannot open '%s' for writing\n", output);
            sa_bands_free(bands, nb);
            sa_dataset_free(&ds);
            return 1;
        }
        close_out = 1;
    }

    int rc = sa_render_svg(&ds, bands, nb, &opts, out);
    if (close_out && fclose(out) != 0 && rc == 0) {
        fprintf(stderr, "error: failed to write '%s'\n", output);
        rc = 1;
    }
    if (rc != 0) {
        fprintf(stderr, "error: render failed\n");
    }

    sa_bands_free(bands, nb);
    sa_dataset_free(&ds);
    return rc == 0 ? 0 : 1;
}
