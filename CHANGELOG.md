# Changelog

All notable changes to pCode Editor will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [0.4.3] - 2024-04-19

### Fixed
- Insert mode typing - InputQueueCharacters was incorrectly cleared in Insert mode, preventing text input
- Added mode check before clearing keyboard input queue

## [0.4.2] - 2024-04-19

### Fixed
- Vim insert mode keys (i, a, I, A, o, O) now properly enter Insert mode
- Text objects (diw, daw, ciw) preserved working

### Verified
- gg and G work correctly

## [0.4.1] - 2024-04-19

### Fixed
- Status bar scrollbar overlap - status bar now accounts for horizontal scrollbar height
- Menu activation with SetWindowFocus
- Cursor visibility with window focus

## [0.4.0] - 2024-04-19

### Added
- Full Vim motions spec compliance implemented:
  - Screen motions: H, M, L, zz, zt, zb, z Enter, z-
  - Page motions: Ctrl+B/F, Ctrl+U/D
  - Search: f/F/t/T, ; and , repeat
  - Paragraph/sentence: {, }, (, )
  - Word motions: ge, g_
  - Text objects: iw, aw, i(/a(, i{/a{, etc.
  - VisualBlock mode: Ctrl+v
  - Operator-motion combos: dw, db, de, d$, d0, yw, etc.
  - g~, gU, gu operators
- Status bar enabled by default
- Window decorations fixed

### Changed
- Version format updated to follow semantic versioning

## [0.3.0] - 2024-04-16

### Added
- Initial release with basic Vim motions
- ImGuiColorTextEdit integration
- Multi-tab support
- Split windows
- File explorer
- Git integration
- Find and replace
- Vim mode toggle

[Unreleased]: https://github.com/djwisdom/pcode-editor/compare/v0.4.3...HEAD
[0.4.3]: https://github.com/djwisdom/pcode-editor/compare/v0.4.2...v0.4.3
[0.4.2]: https://github.com/djwisdom/pcode-editor/compare/v0.4.1...v0.4.2
[0.4.1]: https://github.com/djwisdom/pcode-editor/compare/v0.4.0...v0.4.1
[0.4.0]: https://github.com/djwisdom/pcode-editor/compare/v0.3.0...v0.4.0
[0.3.0]: https://github.com/djwisdom/pcode-editor/releases/v0.3.0