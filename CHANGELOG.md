# Changelog

All notable changes to this project will be documented in this file.

## [0.2.0] - 2026-01-10

### Added
- Fixed-capacity event queue system (`wen_event_queue`) using a ring buffer.
- `WEN_EVENT_QUEUE_CAP` macro to configure maximum queued events.
- Explicit `wen_handshake_status` enum for handshake progression.
- `WEN_WARN_DEPRECATED` opt-in macro for deprecated API warnings.
- `WEN_TODO` and `WEN_UNREACHABLE` runtime diagnostics.
- Transmit buffering via `tx_len` with deferred flush semantics.
- Arena snapshot tracking (`wen_arena_snapshot`) for precise slice lifetime control.
- Slice size limiting via `WEN_MAX_SLICE`.
- Assertion-based API misuse detection (`WEN_ASSERT`) for:
  - polling with an outstanding slice
  - releasing without a slice
  - invalid arena resets

### Changed
- `wen_poll()` now:
  - flushes pending TX data before attempting reads
  - queues events instead of returning them immediately
  - delivers **at most one event per call**
- Handshake processing now integrates with the event queue.
- Slice delivery is strictly queued and never returned directly.
- RX/TX buffer management is incremental and deterministic.
- Close handling is explicit and event-driven rather than implicit.
- Arena memory is released on `WEN_EV_CLOSE` transition.

### Fixed
- Incorrect handling of partial writes.
- Arena rollback on failed event enqueue.
- Undefined behavior when polling with pending TX data.
- Close event emission ordering (close is queued, then delivered on subsequent poll).
- State inconsistencies when EOF occurs with pending slices or TX data.

## [0.1.0] - 2026-01-09

### Initial Release
- Initial public release.
- Deterministic, allocation-free networking core.
- User-provided transport abstraction (wen_io).
- Arena-backed slice lifetime model.
- Basic link state machine and polling API.
- Codec interface scaffolding (handshake / decode / encode).
- Minimal slice delivery (one slice per poll).
- No framing, no event queue, no incremental decoding.

