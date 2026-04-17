# pcode-editor User Guide

Welcome to pcode-editor, a lightweight, Vim-inspired code editor built with Dear ImGui.

## Version

This guide covers **pcode-editor version 0.2.28**.

---

## Getting Started

### Launching the Editor

```bash
# Windows
.\build\Release\pcode-editor.exe

# Linux/macOS/BSD
./build/pcode-editor
```

### Opening Files

- **Command Line**: `pcode-editor filename.cpp`
- **Menu**: File > Open (Ctrl+O)
- **Drag & Drop**: Drop files onto the editor window

---

## Interface Overview

```
+------------------------------------------------------------------+
| File Edit View Help                                           [_][X] |
+------------------------------------------------------------------+
| [+][~]  untitled                                            [X]    |
+------------------------------------------------------------------+
|                                                   |                    |
| 1  #include <stdio.h>                               | 1  #include    |
| 2                                                   | 2             |
| 3  int main() {                                      | 3  int main   |
| 4      printf("Hello\n");                            | 4      print  |
| 5      return 0;                                    | 5      retur  |
| 6  }                                                | 6      }      |
|                                                   |                |
|                                                   +------------+
+------------------------------------------------------------------+
| NORMAL | Ln 1, Col 1 | UTF-8 | LC | 100% | main.cpp | 0.2.28      |
+------------------------------------------------------------------+
```

### Key Interface Elements

| Element | Description |
|---------|-------------|
| **Tab Bar** | Shows open files; click to switch, drag to reorder |
| **Editor Area** | Main code editing area with syntax highlighting |
| **Minimap** | Right-side overview; click to navigate |
| **Gutter** | Left margin with line numbers and change indicators |
| **Status Bar** | Vim mode, cursor position, file encoding, zoom level |
| **Split Windows** | Edit multiple files side-by-side |

---

## Vim Mode

pcode-editor supports Vim-style editing with multiple modes.

### Mode Types

| Mode | Description | Indicator |
|------|------------|-----------|
| **NORMAL** | Default mode for navigation and commands | `NORMAL` |
| **INSERT** | Type to insert text | `INSERT` |
| **COMMAND** | Ex commands (`:w`, `:q`, etc.) | `:` |

### Switching Modes

| Key | Action |
|-----|-------|
| `Esc` | Return to NORMAL |
| `i` | Enter INSERT mode |
| `:` | Enter COMMAND mode |
| `v` | Enter VISUAL mode |
| `V` | Enter VISUAL LINE mode |

### Normal Mode Commands

#### Navigation

| Key | Action |
|-----|-------|
| `h` | Move left |
| `j` | Move down |
| `k` | Move up |
| `l` | Move right |
| `w` | Word forward |
| `b` | Word backward |
| `e` | Word end |
| `0` | Line start |
| `$` | Line end |
| `gg` | File start |
| `G` | File end |
| `:{n}` | Go to line n |

#### Editing

| Key | Action |
|-----|-------|
| `x` | Delete character |
| `dd` | Delete line |
| `yy` | Yank (copy) line |
| `p` | Paste |
| `u` | Undo |
| `Ctrl+r` | Redo |

#### Search

| Key | Action |
|-----|-------|
| `/` | Search forward |
| `?` | Search backward |
| `n` | Next match |
| `N` | Previous match |

---

## Keyboard Shortcuts

### File Operations

| Shortcut | Action |
|----------|--------|
| `Ctrl+N` | New tab |
| `Ctrl+O` | Open file |
| `Ctrl+S` | Save |
| `Ctrl+Shift+S` | Save as |
| `Ctrl+W` | Close tab |
| `Ctrl+Q` | Quit |

### Edit Operations

| Shortcut | Action |
|----------|--------|
| `Ctrl+C` | Copy |
| `Ctrl+V` | Paste |
| `Ctrl+X` | Cut |
| `Ctrl+Z` | Undo |
| `Ctrl+Y` | Redo |
| `Ctrl+F` | Find |
| `Ctrl+H` | Find and replace |
| `Ctrl+A` | Select all |

### View Operations

| Shortcut | Action |
|----------|--------|
| `Ctrl++` | Zoom in |
| `Ctrl+-` | Zoom out |
| `Ctrl+0` | Reset zoom |
| `Ctrl+P` | Command palette |
| `Ctrl+\`` | Toggle terminal |

### Split Windows

| Shortcut | Action |
|----------|--------|
| `:sp` | Horizontal split |
| `:vsp` | Vertical split |
| `Ctrl+W` then `h` | Focus left split |
| `Ctrl+W` then `j` | Focus down split |
| `Ctrl+W` then `k` | Focus up split |
| `Ctrl+W` then `l` | Focus right split |
| `Ctrl+W` then `o` | Close other splits |

---

## Command Palette

Open with `Ctrl+P` or `:palette`.

### Searching

- Type to filter commands
- Arrow keys to navigate
- Enter to execute

### Available Commands

Type these in the palette:

| Command | Action |
|---------|--------|
| `new tab` | Create new file |
| `open` | Open file dialog |
| `save` | Save current file |
| `save as` | Save with new name |
| `close tab` | Close current tab |
| `toggle terminal` | Show/hide terminal |
| `toggle sidebar` | Show/hide file explorer |
| `toggle minimap` | Show/hide minimap |
| `toggle theme` | Switch dark/light |
| `font size` | Open font settings |

---

## Terminal Integration

Open terminal with `:term`, `:shell`, or `:sh`.

### Features

- Direct keyboard input to shell
- Command history with up/down arrows
- Independent font zoom (Ctrl++/-)
- Close with `exit` or X button

### Shell Commands

| Command | Action |
|---------|--------|
| `:term` | Open terminal |
| `:term cmd` | Run specific command |
| `:shell` | Open default shell |
| `:sh` | Open default shell |

---

## Settings

Settings are stored in `pcode-settings.json` in the editor's directory.

### Adjustable Settings

| Setting | Default | Description |
|---------|---------|-------------|
| `font_size` | 18 | Editor font size |
| `font_name` | "" | Font family (empty = system default) |
| `tab_size` | 4 | Tab width in spaces |
| `word_wrap` | true | Enable word wrap |
| `show_line_numbers` | true | Show line numbers |
| `show_minimap` | true | Show minimap |
| `show_spaces` | true | Show whitespace |
| `show_status_bar` | true | Show status bar |
| `dark_theme` | true | Dark/light theme |

### Set Commands

| Command | Action |
|---------|--------|
| `:set nu` | Show line numbers |
| `:set nonu` | Hide line numbers |
| `:set wrap` | Enable word wrap |
| `:set nowrap` | Disable word wrap |
| `:set nu` | Show line numbers |

---

## Bookmarks and Change History

### Bookmarks

| Key | Action |
|-----|-------|
| `Ctrl+Shift+F2` | Toggle bookmark on current line |
| `F2` | Next bookmark |
| `Shift+F2` | Previous bookmark |

### Change History

Changes are tracked in the gutter:
- **Pink** - Modified line
- **Green** - Added line
- **Yellow** - Deleted portion

---

## Code Folding

| Key | Action |
|-----|-------|
| `zc` | Fold content |
| `zo` | Open fold |
| `zR` | Open all folds |
| `zM` | Fold all |
| `za` | Toggle fold |

---

## External Tools

### Open in Terminal

You can open the current file's directory in a terminal:

```bash
:term
```

### Run Commands

Execute commands in the integrated terminal:

```bash
:!make
```

---

## Troubleshooting

### Editor Not Responding

1. Press `Esc` to return to NORMAL mode
2. If frozen, check terminal for errors

### Files Not Saving

- Check file permissions
- Ensure disk space available
- Verify path is correct

### Slow Performance

- Disable minimap (`:set nowrap`)
- Reduce recent files list
- Close unused tabs

---

## License

BSD 2-Clause License - see LICENSE file.