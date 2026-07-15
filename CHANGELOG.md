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
- Bounded pure generated-help validation, exact descriptor-path lookup, top-level index rendering, complete command listing, group pages, and executable-command pages with separate prose and identifier bounds.
- Presentation-layer rejection of control bytes in every help-emitted metadata string, including path tokens, argument names, enum-choice names, and help prose.
- Deterministic generated synopsis and valid-value output from descriptor metadata, including integer, unsigned, compact-float, boolean, enum, string, and secret argument schemas.
- Public explicit-length `bsc_out_write_bytes()` output helper used by generated help rendering.
- Byte-exact LF golden fixtures and host tests for generated help output, visibility filtering, metadata validation, lookup, optional-section omission including groups without descriptions or visible children, short-write boundaries, configured-boundary and capacity-oriented fixtures, exact compact-float precision output, small help-prose limits, and secret non-disclosure.

### Changed

- Active architecture, implementation, test, source-overview, and workflow documents now describe the implemented console/workspace boundary, current repository state, and the distinction between settled, deferred, and unresolved decisions.
- Active documentation now reflects the pure help foundation, the implemented descriptor/help model, separate help metadata validation, static visibility policy, descriptor-order LF output, deferred console built-ins, extended sections, and subtopics, and the historical status of the original handoff.
- Generated-help test sources now comply with the existing function-documentation policy while preserving the validated bounded-help behavior.
