# stacked-area-c

Professional C library and command-line tool to render **stacked area charts
(REAS)** from CSV data, with dependency-free **SVG** output.

[![Build](https://github.com/diogo/stacked-area-c/actions/workflows/ci.yml/badge.svg)](https://github.com/diogo/stacked-area-c/actions/workflows/ci.yml)

## Features

- **Stacked areas**: `sa_stack_compute()` computes, per series, the `y_low`/`y_high`
  band boundaries of the REAS chart (gaps via `NaN` are supported).
- **SVG rendering**: `sa_render_svg()` emits clean, self-contained SVG with
  axes, grid, tick labels, titles and a legend.
- **CSV input**: `sa_dataset_load_csv()` reads the standard "header + data rows"
  format (optional `x` column).
- **No dependencies**: only C99 + the C standard library.
- **Well tested**: unit tests (`test_stacked`, `test_svg`) and an integration
  test for the CLI (`test_cli`), all wired to CTest.
- **CI ready**: GitHub Actions workflow that builds and runs the tests.

## Build

```sh
cmake -S . -B build
cmake --build build
```

## Test

```sh
cd build && ctest --output-on-failure
```

## CLI

```sh
# Render examples/data.csv to out.svg
./build/sa-cli render examples/data.csv -o out.svg --title "Revenue by product"

# With axis labels and without the legend
./build/sa-cli render examples/data.csv -o out.svg -x "Year" -y "Millions (USD)" --no-legend

# To stdout
./build/sa-cli render examples/data.csv | less

# Help
./build/sa-cli --help
```

### CLI options

| Option | Description |
|--------|-------------|
| `-o, --output FILE` | Write the SVG to `FILE` (default: stdout) |
| `-t, --title STR` | Chart title |
| `-x, --x-label STR` | X axis label |
| `-y, --y-label STR` | Y axis label |
| `--no-legend` | Hide the legend |

## Library example

```c
#include "stacked_area.h"

#include <stdio.h>
#include <string.h>

int main(void)
{
    sa_dataset ds;
    sa_dataset_init(&ds);

    if (sa_dataset_load_csv(&ds, "data.csv") != 0)
        return 1;

    sa_band *bands = NULL;
    size_t nb = 0;
    if (sa_stack_compute(&ds, &bands, &nb) != 0)
        return 1;

    sa_svg_opts opts;
    memset(&opts, 0, sizeof opts);
    opts.title = "Revenue by product";
    opts.x_label = "Year";
    opts.y_label = "Millions (USD)";

    sa_render_svg(&ds, bands, nb, &opts, stdout);

    sa_bands_free(bands, nb);
    sa_dataset_free(&ds);
    return 0;
}
```

## Data format (CSV)

- Line 1: header with the series names. If the first token is `x`, column 0
  is used as the x axis; otherwise `x` defaults to the row index.
- Lines 2+: data rows (one value per column).
- Empty/missing tokens are treated as gaps (`NaN`).

Example (`examples/data.csv`):

```csv
x,beta,alpha
2020,4.0,2.0
2021,5.0,3.5
2022,6.2,4.8
2023,7.1,5.5
2024,8.9,6.7
```

## Project structure

```
stacked-area-c/
├── include/stacked_area.h   # public API
├── src/
│   ├── series.c             # sa_dataset, sa_dataset_add, CSV loading
│   ├── stacked.c            # sa_stack_compute, sa_bands_free
│   └── render_svg.c         # sa_render_svg
├── cli/main.c               # sa-cli
├── tests/                   # unit + integration tests (CTest)
├── examples/data.csv        # sample data
├── CMakeLists.txt
├── README.md
├── LICENSE                  # MIT
└── .github/workflows/ci.yml # GitHub Actions CI
```

## License

MIT. See [LICENSE](LICENSE).
