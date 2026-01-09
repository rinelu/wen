# wen

`wen` is a low-level networking library written in C.

It provides a small, explicit API for working with byte streams and protocol codecs.

## Scope

`wen` focuses on:

- Pull-based event processing
- Streaming access to incoming data
- Minimal and predictable memory usage
- Clear ownership rules

`wen` does not provide:

- High-level networking abstractions
- Automatic threading or async runtimes
- HTTP servers or client helpers
- TLS, compression, or reconnection logic

## Memory

Memory behavior is explicit:

- Fixed-size RX/TX buffers
- Optional arena allocator with connection lifetime
- No per-message heap allocation by default

A deterministic build mode is supported.

## Version

Current version: **0.1.0**

The API is incomplete and may change.

## Status

This is an early release.

