# Changelog

All notable changes to this project will be documented in this file.

## 0.3.0 - 2026-01-17

### Added
- Internal `frame_len` tracking to support protocol-aware slice limiting.

### Changed
- Slice emission logic now respects active frame boundaries when a codec reports a frame length.
- Slice length selection prefers remaining frame length over raw RX buffer size.
- Transmit buffer accounting tightened to correctly append encoded output and prevent overflow.
- All WebSocket-related code is now consistently gated behind `WEN_ENABLE_WS`.

### Fixed
- Prevented slices from exceeding frame boundaries during framed protocol decoding.
- Corrected TX buffer growth logic to ensure `tx_len` is incremented only after successful encode.
- Improved safety checks around transmit buffer capacity.

## 0.2.3 - 2026-01-13

### Changed
- Refactored `wen_poll()` internals to simplify control flow and improve readability.
- Removed `WEN_IMPLEMENTATION` define inside the header.
- Updated README.

## 0.2.2 - 2026-01-11

### Bug Fixes
- Fixed test failures on Windows due to platform-specific exec and I/O behavior.
- Corrected version metadata inconsistencies introduced during the previous release.
- Removed unused and redundant code paths discovered during test cleanup.

### Tests & Reliability
- Updated `test_slice_size_limit` to correctly handle oversized payloads.
- Improved fake I/O helpers to enforce buffer capacity and prevent test-time memory corruption.
- Minor test harness cleanups for cross-platform consistency.

## 0.2.1 - 2026-01-10

### Bug Fixes
- Fixed wen_poll behavior when RX buffer is empty and read returns `-1`, which previously caused `EV_ERROR` events and potential segmentation faults in tests.
- Corrected test harness fake_read to return `0` at `EOF`` instead of `-1`.
- Fixed slice handling for frames larger than `WEN_MAX_SLICE` to prevent arena allocation overflow.
- Ensured wen_send correctly sets `tx_len` without prematurely asserting `0`, aligning tests with internal TX flush behavior.

### Tests & Reliability
- Updated `test_slice_size_limit` to properly handle multi-slice frames exceeding WEN_MAX_SLICE.
- Updated `test_tx_flush_before_rx` to check TX flush behavior without assuming immediate `tx_len == 0`.
- Improved polling loops in tests to handle all event types (`EV_OPEN`, `EV_SLICE`, `EV_ERROR``, `EV_CLOSE`) correctly.
- Minor clarifications in arena allocation assertions.

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

