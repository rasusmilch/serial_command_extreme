# Test Strategy

## Purpose

This file is the testing-policy anchor for `serial_command_extreme`.

The project is a reusable bounded embedded serial command library. The platform-independent C99 core is tested on a normal host machine so reviewers can validate tokenizer, registry, matcher, parser, dispatch, console orchestration, generated help, and extended-help catalog behavior without Arduino, ESP-IDF, UART hardware, RTOS services, or firmware flashing.

## Current status

The current core includes bounded tokenizer, static registry validation, longest-path matching, typed positional parsing, selected-command dispatch/access enforcement, output-neutral complete-line execution, pure generated-help validation/lookup/rendering, optional `help`/`commands` complete-line built-ins, and Task 11C-1 extended-help catalog schema plus structural validation.

Task 11C-1 catalog tests cover borrowed catalog pointer/count rules, authoritative descriptor-pointer references, flat non-executable topic metadata, notes/warnings text lists, deterministic presentation example validation, related-command references, visibility-independent structure validation, and the guarantee that catalog metadata does not invoke handlers or access callbacks. Task 11C-2 remains responsible for extended rendering and pure topic pages. Task 11C-3 remains responsible for any explicitly approved catalog-aware console grammar. Runnable example applications remain Phase 4 work.

## Current and future layout

The current host tests are registered through CTest and the self-contained `sce_host_tests` runner. The suite includes module-specific C sources under `test/`, byte-exact generated-help fixtures under `test/golden/`, and the Python forbidden-pattern check when Python3 is available.

Future tests should continue separating host behavior, golden-output checks, static forbidden-pattern checks, adapter compile checks, and hardware validation. Core PR validation should not require target hardware; hardware validation belongs to adapter and integration tasks.

## Required testing posture

Receipts should report default and float-disabled host builds, focused capacity overrides when relevant, warnings-as-errors and sanitizer checks when supported, golden compatibility, and `python3 tools/check_forbidden_patterns.py src`. There is no pytest suite in the current C project.
