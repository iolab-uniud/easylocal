# Changelog

All notable changes to EasyLocal++ will be documented in this file.

The project uses semantic versioning. Release entries are prepared from the
commits since the previous release and are reviewed manually before tagging.

### S20 — Simulated Annealing promotion

Simulated Annealing e' ora API pubblica header-only sotto `easylocal::search`.

- il Runner accetta un NeighborhoodExplorer core con `make_move`; l'enumerazione
  `moves()` e il proposal stocastico `random_move()` sono capability separate;
- `SimulatedAnnealing` usa `random_move(solution, rng) -> std::optional<Move>` e
  restituisce il best-so-far;
- temperature e acceptance sono policy statiche possedute per valore, senza
  metodi virtuali;
- il contratto `temperature_policy` e' `reset()`, `temperature()`,
  `on_iteration(bool accepted)`, `finished()`;
- sono disponibili `temperature::Classic`, `temperature::FixedLength`,
  `temperature::Cutoff` e `temperature::Hybrid`; le ultime tre
  rappresentano rispettivamente le strategie TL1/TL2/TL3 studiate durante S20,
  con redistribuzione del budget residuo nella policy ibrida;
- `MetropolisAcceptance` usa direttamente il costo numerico come energia e
  accetta sempre miglioramenti e uguaglianze;
- `SimulatedAnnealing` rifiuta a compile time costi strutturati, gerarchici o
  lessicografici nel contratto corrente;
- aggiunto l'MWE Exam Timetabling con tre componenti di costo, weighted sum e
  tre delta evaluator;
- aggiunti test deterministici di capability, temperature policy, Metropolis,
  best-so-far, delta e diagnostica compile-fail del costo non numerico.
