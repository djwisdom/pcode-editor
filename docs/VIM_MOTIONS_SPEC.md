# Vim-Style Motions Specification for pcode-editor

## 1. Overview

This document defines the complete specification for implementing Vim-style motions in the pcode-editor. The goal is exact compatibility with Vim's motion behavior, including integration with operators and text objects.

---

## 2. Basic Motion Categories

### 2.1 Character Motions (Single Character)

| Key | Description | Behavior |
|-----|-------------|----------|
| `h` | Left | Move cursor one character left |
| `j` | Down | Move cursor one line down |
| `k` | Up | Move cursor one line up |
| `l` | Right | Move cursor one character right |

**Edge Cases:**
- `h` at line start: Stay at current position (column 0)
- `l` at line end: Stay at last character
- `j`/`k` at first/last line: Stay on current line

---

### 2.2 Word Motions

| Key | Description | Behavior |
|-----|-------------|----------|
| `w` | Word forward | Move to start of next word |
| `b` | Word backward | Move to start of previous word |
| `e` | Word end forward | Move to end of current/next word |
| `ge` | Word end backward | Move to end of previous word |

**Word Definition:**
- Word: Sequence of letters, digits, underscores
- Non-word: Everything else (spaces, punctuation, etc.)

**Edge Cases:**
- `w` at line end: Move to next line's first word
- `b` at line start: Move to previous line's last word
- `e` at line end: Stay at last character
- `b` on first character of word: Move to previous word

---

### 2.3 Line Motions

| Key | Description | Behavior |
|-----|-------------|----------|
| `0` | Line start | Move to column 0 (first character) |
| `^` | First non-blank | Move to first non-blank character |
| `$` | Line end | Move to last character of line |
| `g_` | Last non-blank | Move to last non-blank character |

---

### 2.4 Document Motions

| Key | Description | Behavior |
|-----|-------------|----------|
| `gg` | Document start | Move to first line, column 0 |
| `G` | Document end | Move to last line |
| `nG` | Go to line | Move to line n (e.g., `5G` = line 5) |

---

### 2.5 Screen Motions

| Key | Description | Behavior |
|-----|-------------|----------|
| `H` | Home | Move to first visible line |
| `M` | Middle | Move to middle visible line |
| `L` | Last | Move to last visible line |
| `z Enter` | Redraw top | Current line becomes first visible |
| `z-` | Redraw bottom | Current line becomes last visible |
| `zt` | Redraw top | Current line at top |
| `zb` | Redraw bottom | Current line at bottom |
| `zz` | Redraw center | Current line at center |

---

### 2.6 Horizontal Scroll Motions

| Key | Description | Behavior |
|-----|-------------|----------|
| `zH` | Half screen left | Scroll half screen width left |
| `zL` | Half screen right | Scroll half screen width right |
| `zh` | Screen left | Scroll one character left |
| `zl` | Screen right | Scroll one character right |

---

### 2.7 Search Motions

| Key | Description | Behavior |
|-----|-------------|----------|
| `f{char}` | Find forward | Move to next occurrence of char |
| `F{char}` | Find backward | Move to previous occurrence of char |
| `t{char}` | Till forward | Move to position before next char |
| `T{char}` | Till backward | Move to position after previous char |
| `;` | Repeat last `f/F/t/T` | Repeat in same direction |
| `,` | Reverse repeat | Repeat in opposite direction |

**Edge Cases:**
- `f` with no match: No movement, no error
- `;` with no last search: No movement
- `f` on last occurrence: Stay at that position

---

### 2.8 Paragraph Motions

| Key | Description | Behavior |
|-----|-------------|----------|
| `{` | Paragraph backward | Move to previous blank line |
| `}` | Paragraph forward | Move to next blank line |
| `(` | Sentence backward | Move to previous sentence start |
| `)` | Sentence forward | Move to next sentence start |

**Definitions:**
- Paragraph: Block of text separated by one or more blank lines
- Sentence: Ends with `.`, `!`, `?` followed by whitespace or EOL

---

### 2.9 Window Motions

| Key | Description | Behavior |
|-----|-------------|----------|
| `Ctrl+b` | Page up | Move one page up |
| `Ctrl+f` | Page down | Move one page down |
| `Ctrl+u` | Half page up | Move half page up |
| `Ctrl+d` | Half page down | Move half page down |

---

## 3. Text Objects

### 3.1 Inner Text Objects

| Key | Description | Selection |
|-----|-------------|-----------|
| `iw` | Inner word | Current word |
| `aw` | A word | Current word + trailing whitespace |
| `iW` | Inner WORD | Current WORD |
| `aW` | A WORD | Current WORD + trailing whitespace |
| `is` | Inner sentence | Current sentence |
| `as` | A sentence | Current sentence + trailing space |
| `ip` | Inner paragraph | Current paragraph |
| `ap` | A paragraph | Current paragraph + trailing blank line |
| `i(` / `i)` | Inner parentheses | Inside `(...)` |
| `a(` / `a)` | A parentheses | `(...)` including parens |
| `i[` / `i]` | Inner brackets | Inside `[...]` |
| `a[` / `a]` | A brackets | `[...]` including brackets |
| `i{` / `i}` | Inner braces | Inside `{...}` |
| `a{` / `a}` | A braces | `{...}` including braces |
| `i<` / `i>` | Inner angle brackets | Inside `<...>` |
| `a<` / `a>` | A angle brackets | `<...>` including brackets |
| `i"` | Inner double quote | Inside `"..."` |
| `a"` | A double quote | `"..."` including quotes |
| `i'` | Inner single quote | Inside `'...'` |
| `a'` | A single quote | `'...'` including quotes |
| `` i` `` | Inner backtick | Inside `` `...` `` |
| `` a` `` | A backtick | `` `...` `` including backticks |

### 3.2 Difference Between Inner and A

- **Inner (`i`)**: Selects only the content, excluding delimiters
- **A (`a`)**: Selects content AND the delimiters

---

## 4. Operator Integration

### 4.1 Operator List

| Key | Operator | Action |
|-----|----------|--------|
| `d` | Delete | Delete text |
| `y` | Yank | Copy text |
| `c` | Change | Delete and enter insert mode |
| `g~` | Toggle case | Toggle case (upper/lower) |
| `gU` | Uppercase | Convert to uppercase |
| `gu` | Lowercase | Convert to lowercase |
| `>` | Shift right | Increase indentation |
| `<` | Shift left | Decrease indentation |
| `=` | Format | Auto-indent |
| `!` | Filter | Run external command |

### 4.2 Operator-Motion Rules

**Syntax:** `{operator}{motion}`

**Examples:**
- `dw` - Delete word
- `d$` - Delete to end of line
- `dgg` - Delete to beginning of document
- `dfx` - Delete until next 'x' (inclusive)
- `dt;` - Delete until semicolon (exclusive)

**Count Syntax:** `{count}{operator}{motion}` or `{operator}{count}{motion}`

- `2dw` = `d2w` = Delete 2 words
- `3j` = Move down 3 lines

### 4.3 Double Operator

| Key | Action |
|-----|--------|
| `dd` | Delete current line |
| `yy` | Yank current line |
| `cc` | Change current line |
| `>>` | Shift line right |
| `<<` | Shift line left |
| `==` | Format line |

### 4.4 Visual Mode Operators

| Key | Action |
|-----|--------|
| `d` | Delete selection |
| `y` | Yank selection |
| `c` | Change selection |
| `r` | Replace all selected characters with same char |
| `~` | Toggle case |
| `U` | Uppercase selection |
| `u` | Lowercase selection |
| `>` | Shift right |
| `<` | Shift left |

---

## 5. Visual Mode

### 5.1 Visual Mode Types

| Mode | Key | Description |
|------|-----|-------------|
| `v` | Character | Select by characters |
| `V` | Line | Select whole lines |
| `Ctrl+v` | Block | Select rectangular blocks |

### 5.2 Visual Mode Motions

Same as Normal mode motions work in Visual mode.

**Special:**
- `o` - Swap selection start and end (character/block)
- `O` - Swap to other corner of rectangle (block only)

---

## 6. Count Handling

### 6.1 Count in Motions

- `5j` - Move down 5 lines
- `3w` - Move forward 3 words
- `10l` - Move right 10 characters

### 6.2 Count in Operators

- `5dd` - Delete 5 lines
- `2dw` - Delete 2 words
- `3cc` - Change 3 lines

### 6.3 Count in Both

- `2d3w` = Delete 6 words (2 * 3)

---

## 7. Edge Cases and Special Behaviors

### 7.1 Empty Buffers
- `G` on empty buffer: Stay at line 0
- `gg` on empty buffer: Stay at line 0

### 7.2 Motions at Buffer Boundaries
- `k` on line 0: Stay at line 0
- `w` past last word: Stay at last position
- `$` on single line: Stay at last character

### 7.3 Search Failures
- `f`, `F`, `t`, `T` with no match: No movement, no error
- `;` without prior `f/F/t/T`: No movement

### 7.4 Line Wrapping
- `j` and `k` with wrap enabled: Move to next physical line
- `j` and `k` with wrap disabled: Move to next display line

### 7.5 Multibyte Characters
- Motions account for UTF-8 multibyte characters
- `l` on multibyte: Move by visual character, not byte

### 7.6 Special Characters in Search
- `f;` - Find semicolon
- `f` + `Esc` - Cancel search (no movement)
- `f` + `Ctrl+c` - Cancel search

---

## 8. Implementation Guidelines

### 8.1 State Machine

```
Normal Mode
    |
    +-- Operator Pending (after d,y,c, etc.)
    |       |
    |       +-- Motion (after operator)
    |
    +-- Visual Mode (after v, V, Ctrl+v)
    |
    +-- Insert Mode (after i, a, o, etc.)
```

### 8.2 Key Handling

1. **Read key**
2. **Check mode**
3. **If Normal:**
   - Check for operator (d, y, c, etc.)
   - Check for motion
   - Check for special (gg, ZZ, etc.)
4. **If Operator Pending:**
   - Wait for motion
   - Apply operator with motion
5. **If Visual:**
   - Extend selection with motion
   - Apply operator to selection

### 8.3 Required State Variables

```cpp
enum class Mode { Normal, Insert, Visual, VisualLine, VisualBlock, OperatorPending };

Mode current_mode;
char pending_operator;  // 'd', 'y', 'c', etc.
int motion_count;       // Numeric prefix
int selection_start_x, selection_start_y;
int selection_end_x, selection_end_y;
std::string last_search_char;  // For ; and ,
char last_find_char;
bool last_find_reverse;
```

---

## 9. Keyboard Shortcuts Reference

### 9.1 Mode Switching

| Key | Mode | Description |
|-----|------|-------------|
| `i` | Insert | Insert before cursor |
| `I` | Insert | Insert at line start |
| `a` | Insert | Insert after cursor |
| `A` | Insert | Insert at line end |
| `o` | Insert | New line below |
| `O` | Insert | New line above |
| `R` | Replace | Replace mode |
| `v` | Visual | Character visual |
| `V` | Visual | Line visual |
| `Ctrl+v` | Visual | Block visual |
| `Esc` | Normal | Exit to normal mode |

### 9.2 Normal Mode Commands

| Key | Description |
|-----|-------------|
| `:` | Command line |
| `/` | Search forward |
| `?` | Search backward |
| `n` | Next search match |
| `N` | Previous search match |
| `u` | Undo |
| `Ctrl+r` | Redo |
| `.` | Repeat last command |

---

## 10. Testing Checklist

### 10.1 Basic Motions
- [ ] `h`, `j`, `k`, `l` - Character movement
- [ ] `w`, `b`, `e`, `ge` - Word movement
- [ ] `0`, `^`, `$`, `g_` - Line movement
- [ ] `gg`, `G` - Document movement
- [ ] `H`, `M`, `L` - Screen movement

### 10.2 Search Motions
- [ ] `f`, `F`, `t`, `T` - Character search
- [ ] `;` - Repeat search
- [ ] `,` - Reverse repeat

### 10.3 Operators
- [ ] `d` - Delete
- [ ] `y` - Yank
- [ ] `c` - Change
- [ ] `dd`, `yy`, `cc` - Line operators

### 10.4 Text Objects
- [ ] `iw`, `aw` - Word text objects
- [ ] `i(`, `a(` - Parenthesis text objects
- [ ] `i{`, `a{` - Brace text objects
- [ ] `i"`, `a"` - Quote text objects

### 10.5 Visual Mode
- [ ] `v` - Character visual
- [ ] `V` - Line visual
- [ ] `Ctrl+v` - Block visual

---

## 11. Version History

- v1.0 (2026-04-16) - Initial specification

---

*This specification is designed for exact Vim compatibility in pcode-editor.*