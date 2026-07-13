# Changelog

This file records notable repository changes.

## Unreleased

### Added

- Output-neutral complete-line console orchestration that coordinates explicit-length caller input through the existing tokenizer, longest-path matcher, and selected-command dispatcher.
- Caller-owned bounded `bsc_console_workspace_t` execution storage for line, token, match, parsed-argument, and diagnostic state.
- Explicit-length console input handling with line-length validation and embedded-NUL rejection before tokenization.
- Workspace-local recursion protection for ordinary same-workspace reentrant execution attempts.
- Optional non-secret `bsc_console_result_t` metadata for coarse execution phase, selected command, exact group result, and structured parser diagnostics.
- Complete-line console host-test coverage for initialization, input boundaries, pipeline success and failure paths, output forwarding, workspace cleanup, and recursion-guard behavior.

### Changed

- Active architecture, implementation, test, source-overview, and workflow documents now describe the implemented console/workspace boundary, current repository state, and the distinction between settled, deferred, and unresolved decisions.
