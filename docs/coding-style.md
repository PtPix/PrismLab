# Coding style notes

This is a living record of the repository owner's demonstrated preferences. Add a
rule only after it appears in an explicit edit or the owner confirms it. Keep
language-specific conclusions scoped to that language.

## CMake

Observed on 2026-10-07 in the root and framework CMake files.

- Separate conceptual sections with one blank line. A section may contain several
  closely related targets; blank lines express responsibility boundaries rather
  than mechanically separating every command.
- Keep short commands on one line when their arguments remain easy to scan.
- Format commands containing several source files or shader entries as vertical
  lists with one item per line.
- Indent continued arguments by four spaces.
- Keep the closing parenthesis after the final list item rather than placing it on
  a separate line.
- Use brief English comments to explain platform constraints or label a configuration
  block when the structure alone does not communicate its purpose.

## Not established yet

- HLSL layout and naming preferences.
- Whether CMake comments should explain only constraints or also ordinary sections;
  the current evidence supports both uses but is still limited.

## C++

Confirmed by the repository owner on 2026-10-07: follow the Epic C++ Coding
Standard where it applies to this standalone renderer.

- Use PascalCase for types, functions, variables, and namespaces.
- Prefix ordinary classes and structs with `F`, interfaces with `I`, enums with
  `E`, and template types with `T`.
- Prefix boolean variables with `b`; do not use Hungarian member prefixes such as
  `m_`.
- Put opening braces on a new line and always brace control-flow bodies.
- Use tabs for leading indentation, displayed at four columns.
- Declare public interfaces before private implementation details.
- Keep Prism's `PRISM_` macro prefix. Do not adopt Unreal-only containers,
  reflection macros, object prefixes, or Epic copyright notices because Prism is
  not an Unreal Engine module.
- Preserve third-party naming at Donut, NVRHI, DXC, Win32, standard-library, and
  other external API boundaries.
- Keep the repository-level `.clang-format` and `.clang-tidy` files aligned with
  these rules so new project code can be checked consistently.
