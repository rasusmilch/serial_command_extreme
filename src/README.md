# Source Directory

This directory is reserved for the platform-independent core library.

Current platform-independent C99 core source exists here for configuration, status diagnostics, borrowed string views, output callbacks, complete-line console configuration and output-neutral orchestration, static descriptor types, bounded tokenization, registry descriptor validation, longest-path command matching, typed positional argument parsing, selected-command dispatch/access enforcement, and internal compact float parsing, and pure generated-help validation/lookup/rendering, and optional complete-line help/commands built-in routing. Do not add Arduino, ESP-IDF, UART, RTOS, or hardware-specific dependencies to this directory.

## Intended core modules

Current implemented modules are:

```text
src/bsc_config.h
src/bsc_args.h
src/bsc_args.c
src/internal/bsc_float_parse.h
src/internal/bsc_float_parse.c
src/bsc_status.h
src/bsc_status.c
src/bsc_string_view.h
src/bsc_string_view.c
src/bsc_output.h
src/bsc_output.c
src/bsc_console.h
src/bsc_console.c
src/bsc_dispatch.h
src/bsc_dispatch.c
src/bsc_help.h
src/bsc_help.c
src/bsc_types.h
src/bsc_tokenizer.h
src/bsc_tokenizer.c
src/bsc_registry.h
src/bsc_registry.c
src/bsc_matcher.h
src/bsc_matcher.c
```

Optional console built-in routing is implemented in `bsc_console.*`; planned future modules still include catalog-aware console grammar, adapters, and examples. Exact future names may change, but the core constraints must remain intact.


## Compact float parser boundary

The internal compact float module accepts only the bounded operator grammar used
by `bsc_args`: optional minus, ASCII digits before an optional decimal point,
and one through `BSC_MAX_FLOAT_FRACTION_DIGITS` fractional digits when the point
is present. Scientific notation, NaN, infinity, hexadecimal floats, locale
syntax, and libc float conversion are intentionally outside the core parser. The
shared `BSC_COMPACT_FLOAT_MAX_MAGNITUDE` contract limits accepted compact input
to `-1000000000.0` through `1000000000.0`; registry validation rejects float
descriptors whose bounds fall outside that domain.

## Core constraints

The core must remain:

- C-first.
- Host-testable.
- Platform-independent.
- Deterministic and bounded.
- Free of heap allocation.
- Free of Arduino `String`.
- Free of STL containers.
- Free of Arduino/ESP-IDF includes.
- Free of hidden stateful menu behavior.

## Expected responsibilities

Core source should implement only the reusable command subsystem. Current responsibilities include bounded token handling, static descriptor metadata, registry validation, longest-path matching, typed positional argument parsing with structured diagnostics, selected-command dispatch/access enforcement, output helpers, and status diagnostics. Implemented responsibilities now include output-neutral complete-line orchestration over tokenizer, matcher, and selected-command dispatch, pure generated-help validation, exact metadata-path lookup, deterministic help rendering, and optional complete-line `help`/`commands` routing. Planned responsibilities still include catalog-aware console grammar and broader redaction:

- Bounded line/token handling.
- Bounded tokenizer with quotes and escapes.
- Command descriptor model.
- Command registry validation.
- Longest-path command matching.
- Typed argument validation.
- Selected-command callback dispatch with access enforcement.
- Output-neutral complete-line console orchestration with caller-owned workspace.
- Output callback helpers.
- Generated help/manpage rendering through the pure help API.
- Optional complete-line `help`, exact-path help, and `commands` routing through `bsc_execute_line_with_builtins()`.
- Error/status code mapping.
- Secret argument redaction.

Platform I/O belongs outside this directory under adapters or examples.

## Documentation and tests

Before editing source files, read:

```text
docs/00_serial_console_library_design_intent.md
docs/01_serial_console_library_roadmap.md
docs/02_serial_console_library_implementation_guide.md
docs/code_documentation_policy.md
docs/prior_art_review.md
docs/test_strategy.md
```

New public C APIs, structs, enums, callbacks, and macros that behave like APIs must receive Doxygen-compatible documentation.

Every source-code task must include host tests for new or changed parser, tokenizer, registry, validation, matcher, dispatch, output, redaction, help, or access behavior. The typed positional parser, compact-float diagnostics, selected-command access enforcement, dispatch, catalog validation, pure flat-topic lookup, catalog-aware command/group rendering, and pure topic-page rendering are implemented; catalog-aware console grammar, adapters, and examples remain future work.


## Extended-help catalog source

`bsc_help_catalog.c` implements Task 11C-1 structural validation for optional borrowed extended-help catalogs declared in `bsc_help.h`. It validates catalog pointer/count pairs, authoritative descriptor references, notes, warnings, presentation examples, related descriptors, and flat topics without rendering, allocation, handler invocation, access callbacks, or matcher/parser participation. `bsc_help_extended.c` implements Task 11C-2 pure flat-topic lookup, catalog-aware command/group rendering, and pure topic-page rendering; `internal/bsc_help_internal.*` contains the narrow shared default-option, static-visibility, already-validated exact-path lookup, and shared output helpers used by ordinary and extended help.
