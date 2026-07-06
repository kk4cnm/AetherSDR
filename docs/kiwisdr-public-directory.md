# KiwiSDR public directory — honest, API-policy-aware access

AetherSDR can populate a picker of public KiwiSDR receivers from the project
directory at `kiwisdr.com/public`. Because AetherSDR connects to a receiver via
its native (WebSocket "external API") protocol, it must respect each operator's
choice about whether that API is allowed — *before* offering or attempting a
connection.

## The operator signal: `ext_api`

Every receiver entry in the public directory publishes an `ext_api` value (it is
also in each receiver's `/status`). It is the operator's **external-API
allowance** — the maximum number of channels open to non-browser API clients:

| `ext_api` | meaning |
|---|---|
| `0` | **External API disabled** — operator wants web-browser use only |
| `1 … users_max-1` | API allowed, but some channels reserved for web users |
| `>= users_max` | all channels open to API |

`src/core/KiwiPublicDirectory.{h,cpp}` reads this and exposes it as
`KiwiPublicReceiver::apiPolicy()` and the honor predicate
`mayConnectViaApi()` (`ext_api > 0`).

## How AetherSDR honors it

- **The receiver picker shows only API-permitted receivers.** Receivers with
  `ext_api == 0` are **filtered out entirely** — they are never presented to the
  user and AetherSDR never attempts a native connection to them.
- AetherSDR identifies **honestly** with an `AetherSDR/<version>` User-Agent and
  **never spoofs a browser** to get past the directory's interactive gate. If an
  operator blocks AetherSDR, that is their answer and it is honored.
- The directory fetch is **strictly manual** — only on an explicit user action
  (e.g. clicking "Browse public receivers"). No background polling, no timed
  refresh, no caching-and-redistribution, no enumeration. One human action = one
  fetch.
- Only the server-published directory (HTML comments) and per-receiver `/status`
  are read — never the KiwiSDR source (clean-room, Principle IV).

## Proof of concept

`tools/kiwi_directory_poc.cpp` (target `kiwi_directory_poc`) performs the honest
fetch and prints the per-operator policy breakdown and the honor decision for
each `ext_api == 0` receiver. `tests/kiwi_public_directory_test.cpp` locks the
parser + policy logic (a web-only receiver must never be API-connectable, and
must be excluded by the picker filter).

```
$ kiwi_directory_poc            # honest live fetch
$ kiwi_directory_poc page.html  # offline parse of a saved directory page
```
