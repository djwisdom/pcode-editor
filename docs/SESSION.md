# pcode-editor Session Notes

## Date: 2026-04-18

---

## Accomplished

### 1. UI Layout Fixed
- **Menu**: Fixed at top (unaffected by scroll)
- **Tabs**: Fixed below menu (unaffected by scroll)  
- **Editor Content**: Scrollable, respects status bar height
- **Status Bar**: Fixed at bottom
- **Sidebar**: 200px default, now resizable

### 2. Version System
- `VERSION` file format: `MAJOR.MINOR.PATCH (hash)`
- `get_version()` reads VERSION file at runtime
- `--version` output: `pCode Editor version 0.2.45 (bb8f0ee)`

### 3. Sidebar Resizable (NEW v0.2.45)
- Implemented draggable splitter using `InvisibleButton`
- Range: 100px - 400px
- Cursor changes to resize direction on hover

---

## File Changes

| File | Change |
|------|--------|
| `src/editor_app.cpp` | Status bar z-order, tabs fixed, sidebar resize |
| `VERSION` | Now includes git hash |
| `docs/versioning.md` | Documented version rules |

---

## Latest Commit
```
v0.2.45 (bb8f0ee): Add resizable sidebar with draggable splitter
```

---

## Outstanding Issues

1. **Window split** - Was working but couldn't reproduce reliably
   - Occurred accidentally with specific viewport conditions
   - Not yet implemented as feature

---

## Version History

| Version | Date | Change |
|---------|------|--------|
| v0.2.28 | 2026-04-17 | Initial sessions |
| ... | ... | ... |
| v0.2.44 | 2026-04-18 | Editor uses remaining space after sidebar |
| v0.2.45 | 2026-04-18 | Resizable sidebar |

---

## ImGui Patterns Used

### Splitter (InvisibleButton)
```cpp
ImGui::InvisibleButton("##Splitter", ImVec2(6, height));
if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
if (ImGui::IsItemClicked(0)) dragging = true;
if (dragging && ImGui::IsMouseDown(0)) { width += MouseDelta.x; }
```

### Child Window for Scrolling
```cpp
ImGui::BeginChild("##Content", ImVec2(width, height), false);
// Contents scroll here
ImGui::EndChild();
```

### Fixed Tab Bar (Outside Scroll)
```cpp
ImGui::BeginTabBar("##Tabs", tab_flags);
// Tabs render here - NOT in child
ImGui::EndTabBar();

ImGui::BeginChild("##Content", ...);  
// Scrollable content
ImGui::EndChild();
```

---