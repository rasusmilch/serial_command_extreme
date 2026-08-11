# Serial Command Extreme Architecture Contract

## Purpose

This document is the current architecture and implementation contract for `serial_command_extreme`. It describes the platform-independent C core that exists in this repository, the boundaries between implemented modules, and the decisions that remain deferred or unresolved for future work.

Historical planning context lives in Git history, PR history, receipts, and explicitly historical handoff material. This document is intended to stand on its own as current architectural truth for maintainers and future implementation tasks.

Last reconciled: 2026-07-17.

## Current implementation state

Current implemented repository state:

- C99 core: implemented as the `serial_command_extreme` static library built from `src/`.
- Build system: CMake is implemented at the repository root and builds the library plus host tests.
- Test runner: CTest is implemented through `test/CMakeLists.txt`; the self-contained C executable is `sce_host_tests`.
- Forbidden-pattern tooling: `tools/check_forbidden_patterns.py` is implemented and registered with CTest when Python3 is available.
- Foundational helpers: status names, string views, and output callback helpers are implemented.
- Tokenizer: bounded in-place tokenization is implemented in `src/bsc_tokenizer.*`.
- Descriptor model: static command, argument, access, flag, and callback descriptor types are implemented in `src/bsc_types.h`.
- Registry validation: complete static descriptor-table validation is implemented in `src/bsc_registry.*`.
- Matcher: longest-path command matching is implemented in `src/bsc_matcher.*`.
- Typed parser: positional argument parsing with structured diagnostics is implemented in `src/bsc_args.*`.
- Compact float parser: internal compact decimal float support is implemented in `src/internal/bsc_float_parse.*` and controlled by `SCE_ENABLE_FLOAT`.
- Selected-command dispatch: access enforcement, typed parsing handoff, handler invocation, and handler-status normalization are implemented in `src/bsc_dispatch.*`.
- Complete-line console orchestration: output-neutral orchestration is implemented in `src/bsc_console.*` using lightweight console configuration plus caller-owned execution workspace.
- Host tests: module-specific host tests cover foundational helpers, tokenizer, descriptor types, registry validation, matcher, typed parser, dispatch/access, complete-line console orchestration, optional built-in help/commands routing, and pure generated-help validation/rendering with byte-exact golden fixtures.
- Generated help/manpages: pure metadata validation, exact descriptor-path lookup, index rendering, command-list rendering, group pages, executable-command pages, pure flat-topic lookup, catalog-aware command/group rendering, pure topic-page rendering, and optional complete-line `help`/`commands` routing are implemented in `src/bsc_help.*`, `src/bsc_help_extended.c`, `src/internal/bsc_help_internal.*`, and `src/bsc_console.*`; catalog-aware console topic grammar remains future work.
- Extended-help catalog foundation: Task 11C-1 schema and visibility-independent structural validation are implemented in `src/bsc_help.h` and `src/bsc_help_catalog.c`; Task 11C-2 pure flat-topic lookup, catalog-aware command/group rendering, and pure topic-page rendering are implemented in `src/bsc_help_extended.c`; catalog-aware console grammar is not implemented.
- Examples: not implemented.
- Arduino adapter: not implemented.
- ESP-IDF adapter: not implemented.
- CI workflows: not implemented in this repository.
- License: not finalized.

## Current module boundaries

### `src/bsc_config.h`

Defines conservative compile-time capacities and feature toggles. Current defaults include command count, enum choices, line length, token count, token length, path depth, argument count, compact-float enablement, compact-float fractional precision, extended-help catalog topic/text/example/related limits, and future output chunk size.

### `src/bsc_status.h/.c`

Defines stable public status values and `bsc_status_name()`. Existing numeric values must not be reordered. New statuses require explicit append-only approval.

### `src/bsc_string_view.h/.c`

Defines borrowed explicit-length string views and deterministic exact / ASCII case-insensitive comparisons. Views do not own storage and do not require NUL termination.

### `src/bsc_output.h/.c`

Defines the platform-independent output callback wrapper and bounded write helpers. The callback accepts explicit byte counts and reports short writes through `BSC_STATUS_OUTPUT_TRUNCATED`. The core owns no UART, Serial object, stdout handle, RTOS object, or sink buffer.

### `src/bsc_types.h`

Defines static descriptor metadata and callback types. Descriptor objects borrow all pointed-to storage. Command descriptor arrays, path arrays, path strings, argument arrays, enum choices, help strings, callbacks, and command contexts remain owned by the application or static descriptor provider.

Access level and hidden/listing state are separate concepts:

- `bsc_access_level_t` represents execution access policy: normal, advanced, factory, or locked.
- `BSC_COMMAND_FLAG_HIDDEN` is current help/list visibility metadata consumed by generated-help filtering according to `bsc_help_options_t`.
- The hidden flag is not an access level and does not by itself deny dispatch.

### `src/bsc_tokenizer.h/.c`

Tokenizes a caller-owned mutable byte span using an explicit length. The tokenizer:

- rejects inputs longer than `BSC_MAX_LINE_LEN`;
- rejects CR and LF;
- supports spaces and tabs as separators;
- supports quoted strings and `\"` / `\\` escapes;
- compacts quoted escape content in place;
- writes token views into caller-owned token storage;
- returns `BSC_STATUS_NO_INPUT` for empty or whitespace-only input;
- performs no command matching, argument parsing, access enforcement, dispatch, output, heap allocation, or hidden scratch allocation.

The low-level tokenizer treats embedded NUL as ordinary byte data because it operates on explicit lengths. The high-level console rejects embedded NUL before tokenization.

### `src/bsc_registry.h/.c`

Validates complete static descriptor tables before normal runtime use. It validates command/group shape, path metadata, duplicate paths, argument schemas, enum metadata, string/secret length ranges, float enablement, access levels, and flags. It performs no tokenization, matching, parsing, access callback invocation, dispatch, output, help rendering, or runtime registration.

### `src/bsc_help_catalog.c`

Implements Task 11C-1 extended-help catalog structural validation for public types declared in `src/bsc_help.h`. The module validates one authoritative borrowed command table through `bsc_registry_validate()`, then validates catalog pointer/count pairs, exact descriptor-pointer membership, duplicate target metadata, notes/warnings text lists, deterministic presentation examples, related descriptor references, and flat non-executable topics. It does not call visibility-dependent help validation, render output, perform public topic lookup, add console grammar, invoke handlers/access callbacks, allocate heap, or retain pointers.


### `src/bsc_help_extended.c`

Implements Task 11C-2 pure flat-topic lookup, catalog-aware command/group rendering, and pure topic-page rendering for public catalog metadata. The module owns `bsc_help_topic_lookup_result_clear()`, `bsc_help_find_topic()`, `bsc_help_render_catalog_path()`, and `bsc_help_render_topic()`, derives the authoritative registry only from `bsc_help_catalog_t`, validates catalog structure and visibility-dependent ordinary help metadata before lookup/output, filters related descriptors by static help visibility, preserves catalog metadata order, and emits no bytes before metadata resolution succeeds. It does not add console grammar, invoke handlers/access callbacks, allocate heap, retain pointers, or use matcher/parser/dispatcher/console behavior.

### `src/internal/bsc_help_internal.h/.c`

Provides narrow private help rules shared by ordinary help and pure topic lookup: default help-option construction, static descriptor visibility evaluation, and already-validated exact visible path lookup. These helpers are not public API and must stay narrower than a generic formatting framework.

### `src/bsc_matcher.h/.c`

Matches tokenizer-produced token views against a static descriptor table. It selects the longest executable command path, returns the remaining positional-token slice, and reports exact group matches with `BSC_STATUS_GROUP_REQUIRES_SUBCOMMAND`. It does not parse arguments, enforce access, invoke handlers, render output, or own workspace storage.

### `src/bsc_args.h/.c`

Parses the remaining positional tokens for one matched executable command. It writes caller-owned parsed results and structured parse diagnostics, clears those outputs on entry/failure, and leaves string/secret values as borrowed views into active token storage. It does not match commands, enforce access, invoke handlers, or own execution workspace.

### `src/bsc_dispatch.h/.c`

Dispatch begins with an already selected executable command. It does not call the matcher. Current dispatch behavior is:

1. clear caller-owned parsed-argument and parse-error storage;
2. validate shallow executable-command fields needed before dispatch;
3. apply access before parsing;
4. parse typed positional arguments through `bsc_parse_command_args()`;
5. invoke the handler exactly once after successful access and parse;
6. normalize invalid handler status values to `BSC_STATUS_APP_ERROR`.

Default access behavior when `command->access_fn` is NULL:

- `BSC_ACCESS_NORMAL`: allowed;
- `BSC_ACCESS_ADVANCED`: allowed;
- `BSC_ACCESS_FACTORY`: denied;
- `BSC_ACCESS_LOCKED`: denied.

When `command->access_fn` is non-NULL, the callback receives application context, the command descriptor, and the required access level; its boolean return decides allow or deny. Dispatch accepts `NULL` application context and `NULL` output. A configured hidden flag remains a metadata/listing flag and is not itself an execution denial.

### `src/bsc_console.h/.c`

The high-level console is output-neutral complete-line orchestration. It binds validated configuration and coordinates existing stages without duplicating their behavior.

`bsc_console_t` is lightweight configuration after successful initialization. It stores:

- borrowed command descriptor table pointer;
- command count;
- borrowed application context;
- by-value copy of the optional `bsc_output_t` wrapper;
- `has_output`;
- `initialized`.

`bsc_console_t` owns no line buffer, token array, matcher result, parsed-argument array, parser diagnostic storage, execution-active state, mutex, lock, heap storage, or hidden scratch.

`bsc_console_workspace_t` is caller-owned and supplied per execution. It owns:

- `line_buffer[BSC_MAX_LINE_LEN + 1u]`;
- `tokens[BSC_MAX_TOKENS]`;
- token count;
- matcher result;
- parsed arguments;
- parse diagnostic;
- non-atomic workspace-local active guard.

`bsc_execute_line()` accepts a const console, caller workspace, explicit input byte span, byte length, and optional non-secret result. It:

1. validates console, workspace, initialized state, input pointer, and input length;
2. rejects overlength input;
3. rejects active reuse of the same workspace before modifying it;
4. rejects embedded NUL bytes before tokenization;
5. marks the workspace active and clears transient metadata;
6. copies exactly the submitted bytes into `workspace->line_buffer` with `memmove()`;
7. appends a trailing NUL outside the tokenizer span;
8. calls `bsc_tokenize_line()`;
9. calls `bsc_match_command()`;
10. derives the remaining argument-token slice;
11. calls `bsc_dispatch_command()`;
12. copies command/group metadata and parser diagnostic metadata into `bsc_console_result_t` when applicable;
13. clears line, token, match, parsed-argument, and parser-diagnostic workspace state before returning.

The console emits no command echo, help, parser error, matcher error, access error, `OK`, `ERR`, or final-result output. When output is configured, it is forwarded to the selected handler through dispatch. The result type intentionally does not include raw line text, token views, parsed arguments, parsed string views, parsed secret views, or a duplicate status field.

Token views and parsed string/secret views borrow workspace line storage only during synchronous execution. Handlers must copy persistent values into their own bounded storage.

The workspace active guard detects ordinary same-workspace recursive misuse. It is not atomic, not a mutex, and not cross-task synchronization. Applications and adapters must serialize shared workspaces, mutable application contexts, command contexts, output sinks, and handler/access state.

## Current implementation sequence

### Phase 2A — Core skeleton and host-test harness: implemented

Implemented files include the root CMake build, core library target, initial source modules, `test/CMakeLists.txt`, `test/test_main.c`, foundational host tests, and the forbidden-pattern checker. The host build uses C99 and CTest.

### Phase 2B — Bounded tokenizer: implemented

Implemented in `src/bsc_tokenizer.*` with host coverage in `test/test_bsc_tokenizer.c`.

### Phase 2C — Descriptor types, registry validation, and longest-path matcher: implemented

Implemented through `src/bsc_types.h`, `src/bsc_registry.*`, and `src/bsc_matcher.*`, with host coverage in `test/test_bsc_types.c`, `test/test_bsc_registry.c`, and `test/test_bsc_matcher.c`.

### Phase 2D — Typed argument parser: implemented

Implemented in `src/bsc_args.*` plus the internal compact-float parser. Host coverage is in `test/test_bsc_args.c` and includes float-enabled and float-disabled behavior.

### Phase 2E — Selected-command dispatch and access enforcement: implemented

Implemented in `src/bsc_dispatch.*`, with host coverage in `test/test_bsc_dispatch.c`.

### Phase 2F — Output-neutral complete-line console orchestration: implemented

Implemented in `src/bsc_console.*`, with host coverage in `test/test_bsc_console.c`.

The implemented boundary is lightweight validated console configuration plus caller-owned workspace. The API accepts an explicit input span and length, rejects embedded NUL, performs one bounded copy into workspace storage, calls tokenizer -> matcher -> selected-command dispatch, exposes optional non-secret result metadata, cleans up transient workspace state, and emits no automatic console output.

### Phase 3A — Pure generated help/manpage foundation: implemented

Implemented in `src/bsc_help.*`, with host and byte-exact LF golden coverage in `test/test_bsc_help.c` and `test/golden/`. The pure help core validates help-specific metadata separately from ordinary registry validation, first verifies the underlying registry schema, resolves exact descriptor paths for groups and executable commands without using the dispatch matcher, and renders top-level indexes, complete visible command lists, group pages, and executable-command pages through `bsc_output_t`. It never invokes command handlers or `command->access_fn`. Output uses descriptor-table order, generated synopsis text, generated valid-value text, LF line endings, no heap allocation, no public help workspace, no full-manpage buffer, and immediate propagation of the first output failure.

Task 11B2 optional console built-ins are implemented through the separate `bsc_execute_line_with_builtins()` composition boundary. Task 11C-1 extended-help catalog schema and structural validation and Task 11C-2 extended rendering/topic APIs are implemented without changing existing console APIs. Catalog-aware console grammar remains future Task 11C-3 work.
Implemented Task 11B2 policy is settled: `bsc_execute_line()` remains application-only, while `bsc_execute_line_with_builtins()` is the separate built-in-aware API. Built-in routing uses exact help paths after tokenization, per-invoked-built-in first-token collision rejection, existing console output only, existing `bsc_status_t` values, and separate static `bsc_help_options_t` visibility.

### Task 11C-1 — Extended-help catalog schema and structural validation: implemented

Implemented in `src/bsc_help_catalog.c`, with public borrowed metadata types and diagnostics in `src/bsc_help.h` and host coverage in `test/test_bsc_help_catalog.c`. The catalog wraps one authoritative command table, validates exact descriptor-pointer references by element equality, checks notes, warnings, presentation examples, related descriptors, and flat single-token topic records, and remains independent of help visibility. It emits no output, allocates no heap, retains no pointers, and never invokes handlers or access callbacks.

### Task 11C-2 — Extended rendering and pure topic APIs: implemented

Implemented in `src/bsc_help_extended.c` and `src/internal/bsc_help_internal.*`, with the minimal public `bsc_help_topic_lookup_result_t`, `bsc_help_topic_lookup_result_clear()`, `bsc_help_find_topic()`, append-only `BSC_STATUS_UNKNOWN_TOPIC`, `bsc_help_render_catalog_path()`, and `bsc_help_render_topic()`. Existing ordinary help renderers and console built-ins remain byte-compatible; no-metadata catalog-aware rendering is byte-identical to ordinary path rendering, and catalog-aware console grammar remains Task 11C-3 future work.


### Phase 4A — Host examples: future

Host examples remain future work after the core help/manpage direction is approved.

### Phase 5A — Arduino adapter: future

Arduino adapter work remains future and must stay outside the platform-independent C core.

### ESP-IDF adapter: future

ESP-IDF adapter work remains future and must stay outside the platform-independent C core.

### AS7331 integration pilot: future

AS7331 integration remains future after the standalone library is proven.

## Approved and implemented decisions

The following decisions are settled in the current repository state:

- C99 is the core language standard.
- CMake and CTest are the primary host build/test path.
- The repository uses a self-contained C host-test runner.
- Static command descriptor tables are the MVP registry model.
- Runtime registration is not part of the current MVP.
- Aliases are not part of the current MVP.
- Optional positional arguments are not part of the current MVP.
- Command path matching uses the implemented ASCII case-insensitive comparison behavior in the matcher.
- Enum matching uses the implemented ASCII case-insensitive comparison behavior in the parser.
- `BSC_STATUS_GROUP_REQUIRES_SUBCOMMAND` is implemented current behavior.
- Complete-line console orchestration is output-neutral.
- Large execution storage is caller-owned through `bsc_console_workspace_t`.
- The current console API uses explicit input byte spans and lengths.
- The high-level console rejects embedded NUL bytes before tokenization.
- The high-level console performs one intentional command-text copy into workspace storage.
- The workspace-local active guard rejects ordinary same-workspace recursion but is not synchronization.
- The current default dispatch access matrix allows normal and advanced commands and denies factory and locked commands when no access callback is present.
- A command access callback, when present, decides allow/deny for the command's required access level.
- `BSC_COMMAND_FLAG_HIDDEN` is a metadata flag distinct from access level and does not by itself deny dispatch.
- Pure generated help APIs use static visibility options: normal and advanced descriptors are visible by default, while factory, locked, and hidden descriptors are filtered unless explicitly included.
- Help output uses descriptor-table order and LF-only bytes.

## Deferred beyond the current MVP or current phase

The following features are deferred and are not blockers for the current implemented MVP:

- runtime command registration;
- aliases;
- optional positional arguments;
- examples;
- Arduino adapter;
- ESP-IDF adapter;
- AS7331 integration;
- completion and hints;
- history and line editing;
- authentication/unlock workflows beyond the existing access callback boundary;
- persistent settings;
- interactive prompts.

## Future decisions requiring approval before implementation

The following unresolved decisions belong to future features and require approval before those features begin:
- automatic diagnostic, echo, redacted-echo, and final-result output policy;
- whether to add a bounded formatted-output helper such as `bsc_out_printf()`;
- runtime-registration design, if runtime registration is later approved;
- license selection and third-party code compatibility.

## Memory and ownership rules

Core source must remain bounded and deterministic:

- no heap allocation in the platform-independent core;
- no Arduino `String` or STL containers in the C core;
- no hidden mutable static workspace;
- no large local line, token, parsed-argument, or matcher arrays in core execution paths;
- no platform mutex or RTOS dependency in the C core;
- all major execution storage is compile-time bounded and caller-owned or caller-provided.

Descriptor metadata is borrowed and must outlive validation, matching, parsing, dispatch, console execution, and any result fields that point back to descriptors.

Workspace line bytes are ordinary-cleared by console cleanup as best-effort privacy hardening. This is not a cryptographic secure-erasure guarantee.

## Output and presentation rules

The current core has output helpers, forwards configured output to handlers, implements pure generated-help rendering through explicit help APIs, and optionally routes complete-line `help`/`commands` through `bsc_execute_line_with_builtins()`. The core still does not implement a general console presentation policy.

Deferred presentation features include:
- command echo;
- redacted echo;
- automatic parser/matcher/access diagnostics;
- automatic `OK` / `ERR` / final-result output;
- golden-output policy for operator-facing text.

## Testing contract

Current host tests are built into `sce_host_tests` and registered with CTest. The test runner includes:

- `test_bsc_tokenizer.c`;
- `test_bsc_types.c`;
- `test_bsc_registry.c`;
- `test_bsc_matcher.c`;
- `test_bsc_args.c`;
- `test_bsc_dispatch.c`;
- `test_bsc_console.c`;
- `test_bsc_console_builtins.c`;
- `test_bsc_help.c`;
- `test_bsc_help_catalog.c`.

CTest also registers `sce_forbidden_patterns` when Python3 is available. Core validation must continue to run without Arduino, ESP-IDF, UART hardware, or target boards.

Generated-help work has byte-exact LF golden-output tests for the pure renderer. Current console built-ins reuse the pure renderer bytes; future extended sections must add or update golden fixtures when their exact output is approved. Task 11C-1 catalog validation is covered by focused host tests for pointer/count policy, descriptor-pointer identity, text/example/topic/related bounds, visibility independence, capacity overrides, and callback non-invocation.

## Current non-goals

The current MVP implements pure generated help/manpages through explicit APIs and optional complete-line `help`/`commands` routing, but does not implement catalog-aware console grammar, examples, adapters, history, completion, line editing, aliases, optional positional arguments, runtime registration, authentication, platform locks, automatic diagnostics, automatic final-result output, packaging, license selection, or CI workflows.
