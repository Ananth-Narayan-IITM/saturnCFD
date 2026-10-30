# examples

Case files that a user can run directly. One directory per solver, camelCase
case names.

```text
examples/
  burgers/
    traveling64.yaml
  heat/
  stokes/
  incompressible/
```

Each case is minimal and documented at the top. The larger validation cases
live in `tests/regression/` so that the example directory stays small.

## Rules

- All values are SI.
- No hidden defaults: a case that relies on a default says so in a comment.
- A new solver adds at least one example.
- `-checkConfig` must pass on every example.
