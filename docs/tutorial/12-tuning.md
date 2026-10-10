# 12. Tuning with irace

Good parameter values depend on the problem and its instances.
[irace](https://mlopez-ibanez.github.io/irace/) searches for them by running
candidate configurations on training instances and comparing their results.
An app using `cli::run` can generate the scenario from its parameter schema,
saving you from describing the same parameters again for the tuner.

irace is an R package: install [R](https://www.r-project.org/), then
`Rscript -e 'install.packages("irace")'`.

## What is tuned

Tuning needs a finite domain for each parameter. Built-in schemas already
provide these for rates and probabilities, such as `cooling_rate` in (0, 1).
Temperatures need a range suited to the scale of your cost; their general
domain allows any positive value.

You can narrow domains for an experiment. Schema conditions and requirements
carry over too: inactive parameters are not tuned, and invalid combinations
are excluded. In `examples/tsp/sa_main.cpp`, the program sets a tuning range
for the initial temperature:

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

`--tuning.irace=DIR` writes the scenario without starting a search. Other
switches on the command line supply the baseline settings for its runs:

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

Initial acceptance is commented out here because calibration is disabled:
`calibration_samples` is fixed at 0. The generated condition is ready if you
later enable tuning for both fields. The temperature requirement becomes a
forbidden combination, using the fixed final temperature:

```text title="parameters.txt"
[forbidden]
# final_temperature must be smaller than initial_temperature
# with every parameter it names tuned: !(runners.sa.temperature.final_temperature < runners.sa.temperature.initial_temperature)
!(0.25 < runners.sa.temperature.initial_temperature)
```

## Running irace

Prepare the generated scenario for your experiment:

1. List the training instances in `instances.txt`.
2. Set the run budget in `scenario.txt`.
3. Select parameters and adjust ranges in `parameters.txt`. Add any extra
   invalid combinations to `[forbidden]`.
4. Run `--tuning.irace=DIR` again. It preserves your scenario edits, checks
   that the selected parameters exist, and refreshes `configurations.txt`.
   Starting values outside the selected ranges are moved into range.

Then run irace in the directory:

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

irace compares a single number per run. `--tuning.print=cost` prints it;
`cost_time` also prints elapsed seconds. Numeric costs are used directly.
Hierarchical costs become `hard * W + soft`, while lexicographic costs weight
each level by a power of `W`. Set `W` with `--tuning.hard_weight` (default
10^9), large enough to exceed every soft cost.

This conversion uses a `double`, which represents integers exactly up to 2^53
(about 9 × 10^15). With the default weight, three or more lexicographic levels,
or a hard cost above about 9 × 10^6, can lose lower-priority information to
rounding. In that case, define `scalar_cost(const Input&, const Cost&)`
alongside the Input to provide a suitable scalar measure.

Cost parameters, such as sum weights, are excluded from tuning: changing
them would change the objective used to compare configurations.

## See also

- [Apps and tools](../reference/app-and-tools.md#tuning-with-irace): the
  `tuning` parameters, the generated files and the cost as one number.

## Next steps

[Chapter 13](13-tester.md) explores the app in the interactive tester.
