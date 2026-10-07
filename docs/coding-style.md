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

- C++ layout, naming, control-flow, and ownership preferences.
- HLSL layout and naming preferences.
- Whether CMake comments should explain only constraints or also ordinary sections;
  the current evidence supports both uses but is still limited.
