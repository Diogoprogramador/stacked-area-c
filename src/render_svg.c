/*
 * render_svg.c - sa_render_svg: renders stacked bands (from sa_stack_compute)
 * as an SVG chart: axes, grid, legend and area paths.
 */
#include "stacked_area.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SA_DEFAULT_W 640.0
#define SA_DEFAULT_H 420.0

static const char *const SA_DEFAULT_COLORS[] = {
    "#1f77b4", "#ff7f0e", "#2ca02c", "#d62728", "#9467bd",
    "#8c564b", "#e377c2", "#7f7f7f", "#bcbd22", "#17becf",
};
#define SA_N_DEFAULT_COLORS \
    ((int)(sizeof SA_DEFAULT_COLORS / sizeof SA_DEFAULT_COLORS[0]))

typedef struct {
    double xmin, xmax, ymin, ymax;
    double W, H;
    double left, top, right, bottom;
    int flip_y;
} sa_view;

static double clampd(double v, double lo, double hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

static void fmt_num(char *buf, size_t n, double v)
{
    snprintf(buf, n, "%.6g", v);
}

static void write_escaped(FILE *out, const char *s)
{
    if (!s)
        return;
    for (const char *p = s; *p; p++) {
        switch (*p) {
        case '&': fputs("&amp;", out); break;
        case '<': fputs("&lt;", out); break;
        case '>': fputs("&gt;", out); break;
        case '"': fputs("&quot;", out); break;
        default: fputc((unsigned char)*p, out);
        }
    }
}

static double band_x(const sa_band *b, size_t i)
{
    const sa_series *s = b->base;
    if (s && s->x && i < s->n)
        return s->x[i];
    return (double)i;
}

/*
 * Range for the view: x from the series, y from the bands (falling back to
 * the series when no bands are given). Zero is included as a baseline when
 * the data is entirely positive or negative.
 */
static void data_range(const sa_dataset *ds, const sa_band *bands, size_t nb,
                       double *xmin_out, double *xmax_out,
                       double *ymin_out, double *ymax_out)
{
    double x0 = 0, x1 = 1, y0 = 0, y1 = 0;
    int have = 0;

    for (size_t s = 0; s < ds->count; s++) {
        const sa_series *ser = &ds->items[s];
        for (size_t i = 0; i < ser->n; i++) {
            double x = ser->x ? ser->x[i] : (double)i;
            double y = ser->y ? ser->y[i] : 0.0;
            if (!have) {
                x0 = x1 = x;
                y0 = y1 = y;
                have = 1;
            } else {
                if (x < x0) x0 = x;
                if (x > x1) x1 = x;
                if (y < y0) y0 = y;
                if (y > y1) y1 = y;
            }
        }
    }

    if (bands && nb > 0) {
        for (size_t b = 0; b < nb; b++) {
            for (size_t i = 0; i < bands[b].n; i++) {
                double lo = bands[b].y_low[i];
                double hi = bands[b].y_high[i];
                double ymin_b = lo, ymax_b = hi;
                if (ymin_b > ymax_b) {
                    double t = ymin_b;
                    ymin_b = ymax_b;
                    ymax_b = t;
                }
                if (!have) {
                    y0 = ymin_b;
                    y1 = ymax_b;
                    have = 1;
                } else {
                    if (ymin_b < y0) y0 = ymin_b;
                    if (ymax_b > y1) y1 = ymax_b;
                }
            }
        }
    }

    if (!have) {
        x0 = 0; x1 = 1; y0 = 0; y1 = 0;
    }
    if (x1 <= x0)
        x1 = x0 + 1;
    if (y1 <= y0)
        y1 = y0 + 1;
    if (y0 > 0)
        y0 = 0;   /* include the zero baseline for positive data */
    if (y1 < 0)
        y1 = 0;   /* include the zero baseline for negative data */

    *xmin_out = x0;
    *xmax_out = x1;
    *ymin_out = y0;
    *ymax_out = y1;
}

static void compute_view(const sa_dataset *ds, const sa_band *bands, size_t nb,
                         const sa_svg_opts *o, sa_view *v)
{
    v->W = (o && o->width > 0) ? o->width : SA_DEFAULT_W;
    v->H = (o && o->height > 0) ? o->height : SA_DEFAULT_H;
    v->left = (o && o->left > 0) ? o->left : 60.0;
    v->top = (o && o->top > 0) ? o->top : 20.0;
    v->right = (o && o->right > 0) ? o->right : 20.0;
    v->bottom = (o && o->bottom > 0) ? o->bottom : 40.0;
    v->flip_y = (o && o->flip_y) ? 1 : 0;

    data_range(ds, bands, nb, &v->xmin, &v->xmax, &v->ymin, &v->ymax);
    if (o && o->x_max > o->x_min) {
        v->xmin = o->x_min;
        v->xmax = o->x_max;
    }
    if (o && o->y_max > o->y_min) {
        v->ymin = o->y_min;
        v->ymax = o->y_max;
    }
}

static int map_point(const sa_view *v, double x, double y,
                     double *sx, double *sy)
{
    double w = v->W - v->left - v->right;
    double h = v->H - v->top - v->bottom;
    if (w <= 0 || h <= 0)
        return -1;
    double xr = (v->xmax > v->xmin) ? (v->xmax - v->xmin) : 1.0;
    double yr = (v->ymax > v->ymin) ? (v->ymax - v->ymin) : 1.0;
    double rx = clampd((x - v->xmin) / xr, 0.0, 1.0);
    double ry = clampd((y - v->ymin) / yr, 0.0, 1.0);
    *sx = v->left + rx * w;
    if (v->flip_y)
        *sy = v->H - v->bottom - ry * h;
    else
        *sy = v->top + ry * h;
    return 0;
}

/*
 * Write the band as a filled polygon: lower boundary left-to-right, then
 * the upper boundary right-to-left.
 */
static void write_path(FILE *out, const sa_view *v, const sa_band *b)
{
    double sx = 0.0, sy = 0.0;
    fputs("M", out);
    for (size_t i = 0; i < b->n; i++) {
        if (i > 0)
            fputs(" L", out);
        map_point(v, band_x(b, i), b->y_low[i], &sx, &sy);
        fprintf(out, " %.2f %.2f", sx, sy);
    }
    for (size_t i = b->n; i-- > 0;) {
        map_point(v, band_x(b, i), b->y_high[i], &sx, &sy);
        fprintf(out, " L %.2f %.2f", sx, sy);
    }
    fputs(" Z", out);
}

static void write_tick_labels(FILE *out, const sa_view *v,
                              double plot_x0, double plot_x1,
                              double plot_y0, double plot_y1)
{
    char nb[32];
    double sx = 0.0, sy = 0.0;
    for (int i = 0; i <= 2; i++) {
        double xv = v->xmin + (v->xmax - v->xmin) * i / 2;
        map_point(v, xv, v->ymin, &sx, &sy);
        fmt_num(nb, sizeof nb, xv);
        fprintf(out,
                "  <line x1=\"%.2f\" y1=\"%.2f\" x2=\"%.2f\" y2=\"%.2f\" "
                "stroke=\"#eee\"/>\n",
                sx, plot_y0, sx, plot_y1);
        fprintf(out,
                "  <text x=\"%.2f\" y=\"%.2f\" font-family=\"sans-serif\" "
                "font-size=\"11\" text-anchor=\"middle\">%s</text>\n",
                sx, sy + 12, nb);
    }
    for (int i = 0; i <= 2; i++) {
        double yv = v->ymin + (v->ymax - v->ymin) * i / 2;
        map_point(v, v->xmin, yv, &sx, &sy);
        fmt_num(nb, sizeof nb, yv);
        fprintf(out,
                "  <line x1=\"%.2f\" y1=\"%.2f\" x2=\"%.2f\" y2=\"%.2f\" "
                "stroke=\"#eee\"/>\n",
                plot_x0, sy, plot_x1, sy);
        fprintf(out,
                "  <text x=\"%.2f\" y=\"%.2f\" font-family=\"sans-serif\" "
                "font-size=\"11\" text-anchor=\"end\">%s</text>\n",
                plot_x0 - 8, sy + 4, nb);
    }
}

int sa_render_svg(const sa_dataset *ds, const sa_band *bands, size_t bands_count,
                  const sa_svg_opts *opts, FILE *out)
{
    if (!ds || !out)
        return -1;

    sa_svg_opts o;
    memset(&o, 0, sizeof o);
    if (opts)
        o = *opts;

    sa_view v;
    compute_view(ds, bands, bands_count, &o, &v);

    int show_legend = !o.hide_legend;
    const char *const *colors = o.colors ? o.colors : SA_DEFAULT_COLORS;
    int ncolors = o.colors ? (int)o.n_colors : SA_N_DEFAULT_COLORS;
    if (ncolors <= 0) {
        colors = SA_DEFAULT_COLORS;
        ncolors = SA_N_DEFAULT_COLORS;
    }

    double plot_x0 = v.left;
    double plot_x1 = v.W - v.right;
    double plot_y0 = v.top;
    double plot_y1 = v.H - v.bottom;
    double axis_y = v.flip_y ? plot_y0 : plot_y1;

    fprintf(out, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    fprintf(out,
            "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"%.0f\" "
            "height=\"%.0f\" viewBox=\"0 0 %.0f %.0f\">\n",
            v.W, v.H, v.W, v.H);
    fprintf(out,
            "  <rect x=\"0\" y=\"0\" width=\"%.0f\" height=\"%.0f\" "
            "fill=\"#ffffff\"/>\n", v.W, v.H);

    if (o.title) {
        fprintf(out,
                "  <text x=\"%.2f\" y=\"18\" font-family=\"sans-serif\" "
                "font-size=\"16\" font-weight=\"bold\" text-anchor=\"middle\">",
                v.W / 2);
        write_escaped(out, o.title);
        fputs("</text>\n", out);
    }

    /* axes */
    fprintf(out,
            "  <line x1=\"%.2f\" y1=\"%.2f\" x2=\"%.2f\" y2=\"%.2f\" "
            "stroke=\"#333\" stroke-width=\"1\"/>\n",
            plot_x0, axis_y, plot_x1, axis_y);
    fprintf(out,
            "  <line x1=\"%.2f\" y1=\"%.2f\" x2=\"%.2f\" y2=\"%.2f\" "
            "stroke=\"#333\" stroke-width=\"1\"/>\n",
            v.left, plot_y0, v.left, plot_y1);

    write_tick_labels(out, &v, plot_x0, plot_x1, plot_y0, plot_y1);

    if (o.x_label) {
        fprintf(out,
                "  <text x=\"%.2f\" y=\"%.2f\" font-family=\"sans-serif\" "
                "font-size=\"12\" text-anchor=\"middle\">",
                (plot_x0 + plot_x1) / 2, v.H - v.bottom + 16);
        write_escaped(out, o.x_label);
        fputs("</text>\n", out);
    }
    if (o.y_label) {
        double x0 = v.left / 2;
        fprintf(out,
                "  <text x=\"%.2f\" y=\"%.2f\" font-family=\"sans-serif\" "
                "font-size=\"12\" text-anchor=\"middle\" "
                "transform=\"rotate(-90 %.2f %.2f)\">",
                x0, plot_y0 + 8, x0, plot_y0 + 8);
        write_escaped(out, o.y_label);
        fputs("</text>\n", out);
    }

    /* areas */
    for (size_t s = 0; bands && s < bands_count; s++) {
        const char *color = colors[s % (size_t)ncolors];
        fputs("  <path d=\"", out);
        write_path(out, &v, &bands[s]);
        fprintf(out,
                "\" fill=\"%s\" fill-opacity=\"0.85\" stroke=\"%s\" "
                "stroke-width=\"0.5\"/>\n",
                color, color);
    }

    /* legend */
    if (show_legend && ds->count > 0) {
        double lx = plot_x0 + 2;
        double ly = plot_y1 + 14;
        for (size_t s = 0; s < ds->count; s++) {
            const char *color = colors[s % (size_t)ncolors];
            fprintf(out,
                    "  <rect x=\"%.2f\" y=\"%.2f\" width=\"10\" height=\"10\" "
                    "rx=\"1\" fill=\"%s\"/>\n",
                    lx, ly - 8, color);
            fprintf(out,
                    "  <text x=\"%.2f\" y=\"%.2f\" font-family=\"sans-serif\" "
                    "font-size=\"11\">",
                    lx + 14, ly + 2);
            write_escaped(out, ds->items[s].name);
            fputs("</text>\n", out);
            ly += 18;
        }
    }

    fputs("</svg>\n", out);
    return 0;
}
