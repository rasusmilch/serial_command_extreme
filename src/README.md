# Source Directory

This directory is reserved for the platform-independent core library.

No implementation source exists yet. Do not add Arduino, ESP-IDF, UART, RTOS, or hardware-specific dependencies to this directory.

## Intended core modules

The implementation guide currently expects core files similar to:

```text
src/bsc_config.h
src/bsc_types.h
src/bsc_tokenizer.h
src/bsc_tokenizer.c
src/bsc_registry.h
src/bsc_registry.c
src/bsc_parser.h
src/bsc_parser.c
src/bsc_dispatch.h
src/bsc_dispatch.c
src/bsc_help.h
src/bsc_help.c
src/bsc_console.h
src/bsc_console.c
```

Exact names may change after the read-only architecture plan, but the core constraints must remain intact.

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

Core source should implement only the reusable command subsystem:

- Bounded line/token handling.
- Tokenizer with quotes and escapes.
- Command descriptor model.
- Command registry validation.
- Longest-path command matching.
- Typed argument validation.
- Callback dispatch.
- Output callback helpers.
- Generated help/manpage rendering.
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

Every source-code task must include host tests for new or changed parser, tokenizer, registry, validation, dispatch, output, redaction, help, or access behavior.
