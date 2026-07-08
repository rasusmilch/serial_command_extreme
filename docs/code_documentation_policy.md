# Code Documentation Policy

## Purpose and scope

This file is the documentation-policy anchor for `serial_command_extreme`.

The project is intended to become a reusable bounded embedded serial command library: a C-first parser, command registry, typed argument validator, dispatcher, and generated help/manpage system with platform adapters for Arduino, ESP-IDF, and host builds.

This policy defines how source code, public APIs, hardware-facing behavior, tests, examples, and Codex-generated changes must be documented. It applies to new code and to existing code touched by a task. It does not require bulk documentation churn unless a task is explicitly scoped as documentation-only.

The policy exists to preserve maintainability across future maintainers, junior developers, Codex sessions, reviewers, and embedded debugging work. It also protects the project from architectural drift: comments and docstrings must explain intent, constraints, memory/lifetime rules, typed parser behavior, and hardware assumptions without turning the library into a generic shell or platform-specific wrapper.

## Audience

Documentation must serve these audiences:

- Future maintainers who need to understand parser, registry, validation, dispatch, output, and adapter behavior without reading every call site.
- Junior developers and Codex sessions making bounded changes under review.
- Reviewers checking whether implementation matches the design anchors, roadmap, and tests.
- Operators and hardware/debug support reading generated help output, examples, logs, or support notes.
- Embedded integrators adapting the core to Arduino, ESP-IDF, host stdio, or later target platforms.

Do not write comments solely for the original author. Assume the next reader does not remember the design conversation.

## Repository languages detected

Current-source inspection for this policy found the project repository in an early anchor/documentation state. The current GitHub repository contains serial console documentation anchors under `docs/`. No implementation source files were found in the repository during this pass.

The uploaded archive and reference-only sources contain or plan for the following relevant languages and file types:

- Markdown: design anchors, roadmap, implementation guide, README files, policy documents, test plans, and integration notes.
- C: planned core library, ESP-IDF-style adapter code, PT100 reference firmware, and host-testable parser/registry/help code.
- C++: planned Arduino adapter, Arduino-oriented reference libraries, and possible host/example code.
- Arduino `.ino`: reference examples and future Arduino examples.
- Python: reference build/test/helper scripts and likely future validation or tooling scripts.

Additional file types exist in the reference archive, including JavaScript, HTML, CSS, PDF, images, DOCX, logs, CSV, and Arduino library metadata. These were checked as archive contents, but they are not currently planned as primary implementation languages for this library. This policy does not define a JavaScript or web-front-end documentation standard unless such code is later added to the project.

## General documentation standard

Document public behavior and non-obvious internal behavior at the right level of abstraction.

Documentation should explain:

- What a module or function is responsible for.
- The bounded memory model: buffer ownership, maximum lengths, token counts, command counts, enum limits, and output chunk limits.
- Whether data is copied, borrowed, or viewed into a caller-owned buffer.
- Whether pointers remain valid only during dispatch or beyond return.
- Units, ranges, canonical forms, and validation failure cases.
- Status/error code meanings and whether the caller or library prints final `OK:`/`ERR:` output.
- Blocking behavior and whether a function may be called from polling, task, ISR-like, or host-test contexts.
- Security or privacy constraints, especially secret argument redaction.
- Embedded assumptions: hardware revision, peripherals, timing, reset, watchdog, stack/static allocation, and any datasheet/errata assumptions when hardware-specific code exists.

Documentation must not contradict parser schemas, command descriptors, tests, or generated help. If a command descriptor says an argument accepts `0.1..50.0`, examples and comments must not imply a broader range.

## Markdown documentation standard

Markdown anchors and guides must be precise enough for Codex tasks and human review.

Use Markdown for:

- Design intent and architecture anchors.
- Roadmaps and phase boundaries.
- API and integration guides.
- Test plans and validation receipts.
- Code documentation policy and other project policies.
- User-facing examples and generated-help references.

Markdown docs should include checked context when they influence implementation decisions. Avoid vague references such as "the usual parser behavior". Prefer explicit command examples, buffer limits, status names, and file paths.

When a Markdown file is an anchor, include its role near the top. When a Markdown file is speculative or a placeholder, say so directly.

## Python standard

Python code in this project is expected to be tooling, tests, code generation, validation helpers, or release scripts. It is not expected to be part of the embedded core.

Python docstrings must be PEP 257-compatible. Use concise one-line docstrings for simple private helpers. Use a fuller Google-style block when the function has non-obvious arguments, return values, exceptions, filesystem effects, subprocess behavior, generated files, or safety checks.

Use Google-style sections when they add clarity:

- `Args:` for parameters whose meaning is not obvious from name and type.
- `Returns:` for nontrivial return values.
- `Raises:` for expected exceptions, validation failures, subprocess failures, or file errors.
- `Side Effects:` for writes, deletes, subprocess calls, environment changes, network access, or generated artifacts.
- `Notes:` for constraints, compatibility, or design assumptions.

Example:

```python
def normalize_command_path(raw_path: str) -> tuple[str, ...]:
    """Return a canonical command path from a whitespace-separated path string.

    Args:
        raw_path: Command path text such as "settings wifi status".

    Returns:
        A tuple of lowercase path tokens suitable for descriptor comparison.

    Raises:
        ValueError: If the path is empty or contains an unsupported token.

    Notes:
        This helper is for host tooling only. The embedded C core must not depend
        on Python normalization rules unless the rules are also implemented and
        tested in C.
    """
```

Simple private helper example:

```python
def _is_blank(line: str) -> bool:
    """Return true when a line is empty or whitespace-only."""
```

Python comments should focus on why a validation exists, why a fixture is shaped a certain way, or why a subprocess/build command is constrained. Do not comment basic Python syntax.

## C standard

The core library is expected to be C-first. Public C APIs require Doxygen-compatible comments.

Use Doxygen-compatible comments for:

- Public headers.
- Public structs, enums, typedefs, callbacks, macros that behave like APIs, and nontrivial functions.
- Functions whose behavior depends on buffer sizes, ownership, lifetime, units, blocking behavior, or error/status codes.
- Internal functions with non-obvious parser, tokenizer, matcher, validation, redaction, or output behavior.

Preferred tags:

- `@brief` for a concise summary.
- `@param` for parameters, including ownership and size requirements.
- `@return` for returned values.
- `@retval` for important status-code cases.
- `@note` for design assumptions, lifetime rules, and non-obvious behavior.
- `@warning` for safety, redaction, overflow, blocking, or hardware hazards.
- `@pre` for caller obligations.
- `@post` for state changes after successful calls.

C documentation must explicitly cover these where applicable:

- Ownership: caller-owned, library-owned, static, stack, borrowed view, or application-persistent storage.
- Buffer sizes: required capacity, maximum line length, maximum token count, maximum token length, output scratch size, command path depth, argument count, and enum choice count.
- Units and ranges: milliseconds, hertz, kilohertz, bytes, counts, percentages, or raw register units.
- Error codes: which status values can be returned and what they mean.
- Blocking behavior: non-blocking polling, synchronous dispatch, handler responsibility, and output callback behavior.
- ISR/thread safety: whether the function can be called from an ISR, task, single-threaded loop, or host test.
- Lifetime rules: especially when string views point into a console line buffer and are valid only during callback dispatch.
- Redaction and secrets: whether values may be printed, echoed, logged, or displayed in help examples.

Public header example:

```c
/**
 * @brief Execute one complete console input line.
 *
 * Tokenizes the input line, matches the longest registered command path,
 * validates typed positional arguments, and dispatches the matched handler.
 *
 * @param console Console context with command registry, app context, output
 *        callback, and bounded line buffer. Must not be NULL.
 * @param line Null-terminated input line. The line is copied into the
 *        console-owned bounded line buffer before tokenization.
 *
 * @retval BSC_STATUS_OK The command matched, arguments validated, and the
 *         handler completed successfully.
 * @retval BSC_STATUS_LINE_TOO_LONG The input line exceeded BSC_MAX_LINE_LEN.
 * @retval BSC_STATUS_UNKNOWN_COMMAND No registered command path matched.
 * @retval BSC_STATUS_INVALID_ARGUMENT_TYPE An argument did not match its
 *         declared schema.
 *
 * @pre console->commands points to a valid descriptor array with
 *      console->command_count entries.
 * @note Parsed string arguments are views into console storage and are valid
 *       only until the next console execution or line-buffer mutation.
 * @warning Secret arguments must be redacted by echo/status/log helpers.
 */
bsc_status_t bsc_execute_line(bsc_console_t *console, const char *line);
```

Enum example:

```c
/**
 * @brief Result code returned by parser, validator, dispatcher, and handlers.
 *
 * Status values are intentionally explicit so parser failures are not collapsed
 * into a generic false result. Public callers may use these values for tests,
 * automation, and final error formatting.
 */
typedef enum {
    /** Command completed successfully. */
    BSC_STATUS_OK = 0,
    /** Input was empty after trimming whitespace. */
    BSC_STATUS_NO_INPUT,
    /** Input exceeded the configured line length. */
    BSC_STATUS_LINE_TOO_LONG,
} bsc_status_t;
```

Macro example:

```c
/**
 * @brief Maximum number of tokens produced from one command line.
 *
 * Increase this only after reviewing stack/static storage impact and tokenizer
 * tests. The tokenizer must return BSC_STATUS_TOO_MANY_TOKENS instead of
 * writing beyond this bound.
 */
#ifndef BSC_MAX_TOKENS
#define BSC_MAX_TOKENS 24u
#endif
```

Do not use Doxygen as a substitute for tests. If a function has a documented rejection case, the rejection case should have a test unless the task explicitly states why it cannot be tested yet.

## C++ standard

C++ is expected in adapters, Arduino wrappers, examples, and possibly host tools. The embedded core should remain C-first unless the architecture plan is explicitly changed.

Use Doxygen-compatible comments for:

- Public classes, methods, functions, templates, and interfaces.
- Public enums and configuration types.
- RAII/ownership types and adapter classes.
- Nontrivial private methods that mediate buffer ownership, platform I/O, polling, command dispatch, or hardware interactions.

C++ documentation must cover:

- Invariants that must hold before and after calls.
- Ownership and lifetime of referenced streams, buffers, callbacks, and application contexts.
- Whether copying or moving is allowed, disabled, or dangerous.
- Error behavior: return status, exceptions, assertions, or platform-specific error handling.
- Thread safety and whether methods are intended for loop/task/ISR-like contexts.
- Side effects such as writes to `Stream`, UART, logs, files, GPIO, timers, queues, or persistent settings.
- Allocation policy. Adapters should not introduce heap allocation unless explicitly approved and documented.

Class example:

```cpp
/**
 * @brief Arduino Stream adapter for the bounded serial console core.
 *
 * The adapter owns only bounded input buffering. It borrows the Stream and core
 * console context supplied by the caller; neither may be destroyed while the
 * adapter is in use.
 *
 * @warning This adapter must not use Arduino String internally unless a future
 *          architecture decision explicitly approves that tradeoff.
 */
class BscArduinoConsole {
public:
    /**
     * @brief Poll the Stream for complete lines and dispatch ready commands.
     *
     * @return Last dispatch status, or BSC_STATUS_NO_INPUT when no complete
     *         line was available.
     * @note Intended for Arduino loop/task context, not ISR context.
     */
    bsc_status_t poll();
};
```

RAII/ownership example:

```cpp
/**
 * @brief Host-test output collector with fixed storage.
 *
 * Copying is disabled so tests do not accidentally duplicate stale buffer views.
 * The collector truncates writes only through explicit status paths; silent
 * truncation is not acceptable for golden help-output tests.
 */
class FixedOutputCapture {
public:
    FixedOutputCapture(const FixedOutputCapture&) = delete;
    FixedOutputCapture& operator=(const FixedOutputCapture&) = delete;
};
```

## Embedded C/C++ standard

Embedded documentation is mandatory when code touches hardware, timing, interrupts, RTOS behavior, watchdogs, reset behavior, persistent storage, or calibration/scaling. Do not invent hardware facts; document only what is known from the source, datasheet, board definition, or project anchor. Mark unknown hardware assumptions as unverified.

Embedded code comments must document applicable items:

- Hardware revision assumptions and board variants.
- Pin maps, alternate functions, pullups/pulldowns, active-high/active-low behavior, and electrical constraints.
- Peripheral maps: UART, I2C, SPI, ADC, timers, GPIO, DMA, watchdog, storage, USB, or display interfaces.
- Clock/timer configuration and derived timing units.
- Interrupt sources, ISR/main-loop interactions, and what state is shared.
- DMA ownership, alignment, cache, lifetime, and completion behavior.
- RTOS tasks, queues, priorities, stack sizes, synchronization, and timeout policy.
- Static allocation, stack allocation, heap prohibition, and memory pressure assumptions.
- Calibration constants, units, scaling, fixed-point math, and conversion formulas.
- Power, reset, brownout, watchdog, sleep/wake, and boot-mode behavior.
- Datasheet, errata, or register-sequence assumptions.
- Timing constraints and safety-critical delays.

Example:

```c
/**
 * @brief Read one complete UART command line into caller-provided storage.
 *
 * @param dst Destination buffer for the received line.
 * @param dst_len Size of dst in bytes, including space for the null terminator.
 * @param timeout_ms Maximum time to wait for a complete line.
 *
 * @retval BSC_STATUS_OK A full line was read and null-terminated.
 * @retval BSC_STATUS_LINE_TOO_LONG Input exceeded dst_len - 1 before newline.
 * @retval BSC_STATUS_NO_INPUT No complete line arrived before timeout.
 *
 * @note This function is platform-adapter code. The core parser must remain
 *       independent of UART drivers and RTOS timing APIs.
 * @warning Not ISR-safe. It may block until timeout_ms expires.
 */
bsc_status_t bsc_uart_read_line(char *dst, size_t dst_len, uint32_t timeout_ms);
```

For `serial_command_extreme`, current hardware-specific facts are unverified because the new standalone repository has not yet implemented Arduino, ESP-IDF, or AS7331 integration code. Future tasks must document hardware assumptions only after inspecting the integration source and board context.

## Inline comment expectations

Inline comments are required when they explain non-obvious intent or prevent future regressions.

Use inline comments for:

- Domain assumptions that are not visible from code structure.
- Parser corner cases, especially quotes, escapes, empty tokens, ambiguous command paths, and longest-path matching.
- Strict rejection cases that prevent quality escapes.
- Redaction paths that prevent secret values from leaking into echo, logs, help examples, status output, or test failure messages.
- Ownership and lifetime risks that cannot be expressed cleanly in the type system.
- Hardware/peripheral/timing/concurrency assumptions.
- Security, safety, privacy, calibration, or regulatory constraints.
- Regression protections: why a test fixture or branch exists because a previous bug or likely quality escape was identified.

Example:

```c
/* Prefer the longest command path before parsing arguments so literal
 * subcommands such as "settings wifi set ssid" cannot be mistaken for a
 * shorter command with extra string arguments. */
```

Example:

```c
/* Keep password as a view into the line buffer only long enough for the
 * application callback to copy it into bounded storage. Never persist this
 * pointer beyond dispatch. */
```

## What not to comment

Do not add comments that merely narrate syntax:

```c
/* Increment i. */
i++;
```

Do not add speculative comments, stale TODOs, or explanations that are not backed by inspected source or approved design. Do not claim a function is ISR-safe, non-blocking, allocation-free, constant-time, or hardware-validated unless the code and tests support that claim.

Do not remove useful product, hardware, safety, parser, memory, redaction, or regression comments unless replacing them with a more accurate explanation.

Do not write comments that hide weak design. If behavior is hard to explain because architecture is confused, fix or flag the architecture instead of adding a vague comment.

Do not duplicate obvious information already encoded in a command descriptor, argument schema, enum name, or test name unless the duplicate text adds context, units, or rationale.

## Codex task requirements

Every Codex source-code task must require reading this policy before editing source files. Prompts must restate the documentation expectations in acceptance criteria, not only reference this file.

Codex must document:

- New public modules, headers, classes, structs, enums, callbacks, macros that behave like APIs, and functions.
- Touched public APIs whose existing comments are missing, stale, or misleading.
- New or touched parser/tokenizer/registry/help/dispatch paths when behavior is non-obvious.
- New or touched tests and fixtures when they protect a regression, quality escape, hardware assumption, or strict rejection case.
- Hardware abstractions and platform adapters when they define ownership, timing, blocking, or memory behavior.

Codex must not perform broad unrelated comment churn in implementation tasks. Documentation changes should be local to the files and behavior touched by the task unless the task is explicitly documentation-only.

Paste-ready acceptance language for future execute tasks:

```text
Documentation acceptance criteria:
- Read docs/code_documentation_policy.md before editing source.
- Add or update Doxygen-compatible comments for new or touched public C/C++ APIs, structs, enums, callbacks, and macros that behave like APIs.
- Add or update Python docstrings for new or touched Python modules/functions using PEP 257-compatible docstrings and Google-style sections where useful.
- Document ownership, buffer sizes, units, lifetime, error/status codes, blocking behavior, thread/ISR safety, and redaction behavior where applicable.
- Add inline comments only for non-obvious design, parser edge cases, memory/lifetime constraints, hardware/timing assumptions, or regression protections.
- Do not add comments that narrate syntax, speculate, or contradict tests, schemas, or generated help.
- Do not remove useful product/hardware/safety comments without replacing them with a more accurate statement.
- In the final receipt, list documentation files/comments changed and any documentation intentionally deferred.
```

## Documentation-only source-pass safety

Documentation-only tasks must preserve behavior. A task that claims to change only comments, docstrings, Markdown, or generated documentation must include behavior-preservation checks appropriate to the language.

For Python documentation-only passes:

- Use AST equivalence after removing docstrings where practical.
- Run available Python tests and lint/static checks when the repo provides them.
- State any skipped checks and why.

For C/C++ documentation-only passes:

- Prefer compile, preprocess, or token-based checks where practical.
- Run available unit tests, host builds, Arduino/ESP-IDF compile checks, or static checks provided by the repo.
- Confirm no source tokens changed outside comments/docstrings when a task is documentation-only and tooling is available.
- State if embedded target compilation is unavailable.

For Markdown-only passes:

- Confirm no source files were changed.
- If links or referenced files are changed, verify paths where practical.
- State whether no build/test run was necessary because source behavior was untouched.

Embedded target and hardware verification limits must be stated. Do not claim hardware behavior was validated unless the task actually ran on hardware or inspected provided hardware logs/receipts.

## Suggested phased backlog

Use these phases when planning dedicated documentation work. Do not mix broad documentation cleanup into unrelated feature tasks unless the touched code requires it.

### Phase D1 — Add or verify behavior-preservation guard tooling

Goal: make documentation-only passes safe.

Potential work:

- Add Python AST docstring-stripping comparison tooling if Python scripts become part of the repo.
- Add C/C++ comment-stripping or token-equivalence checks if practical.
- Add a documented host build/test command once the core exists.
- Add documentation-only validation instructions to the test plan.

Acceptance:

- Documentation-only tasks can prove that behavior did not change, or explicitly state what remains unverified.

### Phase D2 — Document core entry points

Goal: document the first stable C core API surface.

Potential work:

- `bsc_execute_line()` or equivalent top-level dispatcher.
- Tokenizer entry points.
- Registry/matcher entry points.
- Argument validator entry points.
- Help renderer entry points.
- Output callback helpers.

Acceptance:

- Public entry points state ownership, storage limits, status codes, lifetime rules, and blocking behavior.

### Phase D3 — Document public APIs and headers

Goal: make public headers reviewable without reading all implementations.

Potential work:

- Public structs, enums, macros, callbacks, and descriptor models.
- Configuration macros and their memory/test impact.
- Access-check and custom-validator contracts.
- Secret argument redaction requirements.

Acceptance:

- Public headers are Doxygen-compatible and no public behavior relies only on implementation comments.

### Phase D4 — Document hardware abstraction and peripheral code

Goal: document platform adapters without contaminating the core.

Potential work:

- Arduino `Stream` adapter lifetime and polling rules.
- ESP-IDF UART/console adapter ownership, blocking, and RTOS assumptions.
- Host stdio adapter behavior.
- Any future AS7331 or PT100 integration adapter assumptions.

Acceptance:

- Hardware/platform assumptions are explicit and limited to adapter/integration code.

### Phase D5 — Document complex tests and fixtures

Goal: make tests explain the risks they protect.

Potential work:

- Tokenizer quote/escape fixtures.
- Longest-path matching ambiguity tests.
- Secret redaction tests.
- Help golden-output tests.
- Boundary tests for line length, token count, enum count, numeric ranges, and string lengths.

Acceptance:

- Tests expose their intent without duplicating implementation details.

## Policy for anchor files and related docs

This policy is the canonical code-documentation anchor. Future docs may link here, but they should not create competing documentation policies.

If the repository later adds `docs/design_intent.md`, `docs/roadmap.md`, or other renamed canonical anchors, link this file from those anchors rather than duplicating its contents.

If this policy conflicts with a more specific approved task instruction, follow the task instruction for that task and update this policy later if the exception should become general project policy.

## Last updated context and unverified items

Last updated: 2026-07-07.

Context inspected for this policy:

- Local uploaded archive: `/mnt/data/Archive.tar.gz` extracted to `/mnt/data/sce_archive`.
- Local docs from archive: `docs/00_serial_console_library_design_intent.md`, `docs/01_serial_console_library_roadmap.md`, `docs/02_serial_console_library_implementation_guide.md`, and `docs/03_serial_console_library_handoff.md`.
- Reference-only code directories in the archive, including `AdvancedCLI`, `StaticSerialCommands`, `SimpleSerialShell`, `SerialUI`, `SerialCommandCoordinator`, `SerialCommands`, `SerialCommand`, `SerialCommand_Advanced`, `SerialCmd`, `ParseCommands`, `SerialConfigCommand`, `CommandCatcher`, `tinyCommand`, `cmdArduino`, and `PT100_Mesh_Datalogger`.
- PT100 reference policy inspected as a source of documentation-structure precedent only: `reference_only/PT100_Mesh_Datalogger/docs/code_documentation_policy.md`.
- PT100 console reference files noted for future implementation planning: `main/console_registry.h`, `main/console_registry.c`, `main/console_help.h`, `main/console_help.c`, and `main/console_alerts.c`.
- GitHub repository inspected: `rasusmilch/serial_command_extreme`, branch `main`, public repository. The canonical `docs/code_documentation_policy.md` did not exist before this file was created.

Unverified items:

- No implementation source existed in the current repository during this policy pass; source-language standards are based on the approved design anchors and uploaded reference archive.
- No CI, build system, formatter, test runner, or hardware target has been verified for the new standalone repository.
- Arduino, ESP-IDF, AS7331, and other hardware integration assumptions remain unverified until those integrations exist and are inspected.
- Repository code search indexing was reported unavailable during inspection, so file discovery relied on known paths, repository metadata, and the uploaded archive.
