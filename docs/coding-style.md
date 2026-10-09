# Coding style

Follow the applicable Epic C++ conventions for this standalone renderer, using
`algorithms/Surface/DepthRenderer.*` as the local pass example and the repository
`.clang-format` for C++ layout. Keep rules language-specific; preserve existing
third-party APIs and configuration/shader binding names.

## C++

- PascalCase for project types, functions, variables, and namespaces; `F` for
  ordinary classes/structs, `I` for interfaces, `E` for enums, `T` for template
  types, and `b` for booleans. No `m_` on project members; do not rename Donut,
  NVRHI, Win32, or other external API members.
- Four-column tabs for leading indentation; Allman braces for definitions and
  **every** `if`/`else`/loop body, including one-line early returns. Put public
  interfaces before private details; avoid trailing whitespace.
- Use `<framework/...>` / `<algorithms/...>` for project-root headers and
  `"LocalHeader.h"` for neighboring files. For shared CPU/HLSL constant layouts,
  use a guarded header included on both sides and check its size with
  `static_assert`. Small self-contained samples need not share a header.
- In the Depth-style pass, keep inputs, settings, and outputs explicit; reusable
  recording stays in `algorithms`, while scene setup and visualization stay in
  `samples`. This is a separation of responsibilities, not a requirement to split
  every small sample into multiple files.
- Retain Prism's `PRISM_` macro prefix; do not import Unreal-only containers,
  reflection machinery, or copyright notices.

## HLSL

- Include `Prism/Common/Platform.hlsli` before shader declarations; write
  multi-line blocks with Allman braces. Preserve existing entry-point, resource,
  semantic, and binding names. No general HLSL identifier naming rule is set.

## CMake

- Group related commands by responsibility, with a blank line between sections.
  Keep short commands on one line; list multiple sources/shaders one per line.
- Indent continued arguments four spaces, and put `)` after the last item.
  Use short English comments where a constraint or group needs explanation.
