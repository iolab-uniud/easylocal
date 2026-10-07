# 12. Tuning with irace

The best values of a runner's parameters depend on the problem and on its
instances. [irace](https://mlopez-ibanez.github.io/irace/) finds them
automatically: it runs the program on a set of instances with candidate
values, compares the costs it prints and races the candidates until the best
ones are left. A program made with `cli::run` (chapter 11) writes everything
irace needs from its own parameters, so the files never drift from the
switches the program accepts.

irace is an R package: install [R](https://www.r-project.org/), then
`Rscript -e 'install.packages("irace")'`.

## What is tuned

A parameter is tuned when it has a finite domain: the values it may take,
declared in the schema of its block (chapter 9). The built-in runners declare
the domains of their rates and probabilities, such as Simulated Annealing's
`cooling_rate` in (0, 1). A temperature's domain is every positive number,
since its good values depend on the scale of the costs: a program gives it a
finite range for tuning, as it may narrow any domain. The conditions and
requirements of the schema (chapter 9) come along: a parameter is tuned only
when it matters, and irace never proposes values that break a requirement. `examples/tsp/sa_main.cpp`
gives a range to the initial temperature:

<!-- snippet: tsp/sa_main.cpp:tuning -->
```cpp
// --tuning.irace=DIR writes an irace scenario: the parameters with a domain
// are tuned, here also the initial temperature, on a logarithmic scale.
return easylocal::cli::run(
    application,
    argc,
    argv,
    {.defaults =
            {
                .instance = EASYLOCAL_TSP_INSTANCE_FILE,
                .seed = 2026,
                .start = "initial",
            },
        .tuning = {
            {"runners.sa.temperature.initial_temperature",
                easylocal::config::range(1.0, 100.0).log()},
        }});
```

## The scenario

`--tuning.irace=DIR` creates the directory and writes the
scenario, without running anything; the other switches on the same command
line are the values every run starts from:

```text
$ easylocal_tsp_sa --tuning.irace=tuning --runners.sa.temperature.allowed_iterations=20000
wrote tuning/parameters.txt
wrote tuning/fixed.conf
wrote tuning/target-runner
wrote tuning/instances.txt
wrote tuning/scenario.txt
updated tuning/configurations.txt
2 parameters to tune, 3 more to complete in parameters.txt; list the instances in instances.txt; then run irace in tuning
```

| File | |
| --- | --- |
| `parameters.txt` | the parameters to tune, one per line with its switch, type, range and condition; the others are commented out, with a range to start from; the requirements as `[forbidden]` combinations |
| `configurations.txt` | the current values, which irace tries first |
| `fixed.conf` | the values given on the command line, read by every run with `--config` |
| `target-runner` | the script irace calls: it runs the program with `--tuning.print=cost` on an instance, a seed and the candidate values |
| `instances.txt` | the instances to tune on, one per line: the program's default instance to begin with |
| `scenario.txt` | irace's settings: the files above and the budget, `maxExperiments`, the number of runs |

```text title="parameters.txt"
# Multiplicative cooling factor (default 0.75)
runners.sa.temperature.cooling_rate "--runners.sa.temperature.cooling_rate=" r (0.0001, 0.9999)

# Maximum number of annealing iterations (default 20000)
# no finite domain: a range around the default to start from
# runners.sa.temperature.allowed_iterations "--runners.sa.temperature.allowed_iterations=" i,log (2000, 200000)
```

Here the initial acceptance is not tuned: it matters only when
`calibration_samples` is above 0, and that parameter is not tuned and is 0.
Its line is commented out, with the condition irace needs when both are
uncommented. The requirement between the temperatures becomes a forbidden
combination, with the final temperature at its value:

```text title="parameters.txt"
[forbidden]
# final_temperature must be smaller than initial_temperature
# with every parameter it names tuned: !(runners.sa.temperature.final_temperature < runners.sa.temperature.initial_temperature)
!(0.25 < runners.sa.temperature.initial_temperature)
```

## Running irace

The files are a stub to edit: the program never overwrites them. List the
instances in `instances.txt`, set the budget in `scenario.txt`, uncomment a
parameter or change a range in `parameters.txt`, and add to the `[forbidden]`
section the combinations that are not valid and that the program does not
declare. Then run `--tuning.irace=DIR` again: it checks that
every parameter of `parameters.txt` is one of the program's and rewrites
`configurations.txt` to agree with it, moving a current value into its range
when it is outside. Finally, run irace in the directory:

```text
$ cd tuning && irace
...
# Best configurations as commandlines (first number is the configuration ID; listed from best to worst according to the sum of ranks):
11 --runners.sa.temperature.initial_temperature=4.1621 --runners.sa.temperature.cooling_rate=0.6166
30 --runners.sa.temperature.initial_temperature=5.4712 --runners.sa.temperature.cooling_rate=0.6343
```

The best configurations are switches of the program: give them on its command
line, or as `path = value` lines in a configuration file (chapter 9).

## The cost as one number

irace compares runs by one number, which
`--tuning.print=cost` prints (`cost_time` adds the running time in seconds).
A number is itself; a hierarchical cost is `hard * W + soft`, and a
lexicographic one weighs each value by a power of `W`, with `W` from
`--tuning.hard_weight` (10^9 by default), which must exceed every soft cost.
The number is a `double`, exact up to 2^53 (about 9 * 10^15): with the default
weight, a lexicographic cost of three or more levels, or a hard cost above
about 9 * 10^6, loses its lower levels to rounding. A problem can give its own
number with a `scalar_cost(const Input&, const Cost&)` function next to its
Input, found as `read_cost` is, which is the remedy then. The parameters of
the cost, such as the weights of a `cost::sum`, are never tuned: they define
the cost that irace compares.

## See also

- [Apps and tools](../reference/app-and-tools.md#tuning-with-irace): the
  `tuning` parameters, the generated files and the cost as one number.

## Next steps

[Chapter 13](13-tester.md) explores the app in the interactive tester.
