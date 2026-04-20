# ImGui Notification System - Documentation

## Overview

A simplified toast notification system for pcode-editor, inspired by imgui-notify but without external font/icon dependencies.

## Deep Dive Analysis

### External Library Comparison: imgui-notify

**Source**: https://github.com/patrickcjk/imgui-notify
**Maintained Fork**: https://github.com/TyomaVader/ImGuiNotify

| Feature | External Lib | pcode-editor (Before) | pcode-editor (After) |
|---------|-------------|----------------------|---------------------|
| Toast types (4) | ✅ | ✅ | ✅ |
| Title + content | ✅ | ✅ | ✅ |
| Auto-dismiss | ✅ | ✅ | ✅ |
| Fade animation | ✅ (500ms) | ✅ (500ms) | ✅ (300ms) |
| Stacked positioning | ✅ | ✅ | ✅ |
| Theme-aware colors | ❌ | ❌ | ✅ |
| Dismiss button | ✅ (fork) | ❌ | ✅ |
| Dynamic height | ✅ | ❌ | ✅ |
| Max visible limit | ✅ (config) | ❌ | ✅ |
| Vector iteration fix | N/A | ❌ | ✅ |

### Why Keep Custom Implementation?

1. **Zero dependencies**: No Font Awesome, no C++17 requirement
2. **Theme integration**: Colors match pcode-editor's dark theme
3. **Simplicity**: ~190 lines vs ~500+ for external lib
4. **Control**: Full customization without fighting upstream changes

### Pros & Cons

| Aspect | Assessment |
|--------|------------|
| **Simplicity** | ✅ Much simpler than external lib |
| **pcode-editor integration** | ✅ Custom dark theme colors |
| **Dependencies** | ✅ Zero external deps |
| **Customization** | ✅ Configurable via NotificationConfig |
| **Robustness** | ✅ Fixed vector erase during iteration |
| **UX** | ✅ Manual dismiss available |

## Features

- **4 Notification Types**: Success, Warning, Error, Info
- **Auto-dismiss**: Notifications disappear after configurable duration
- **Fade out**: Smooth fade out in last 300ms
- **Stackable**: Multiple notifications stack vertically (max 5 by default)
- **Position**: Bottom-right corner of screen
- **Dismiss button**: Click "x" to close manually
- **Theme-aware**: Colors match pcode-editor dark theme

## Usage

### Including the Header

```cpp
#include "imgui_notify.h"
```

### Adding Notifications

```cpp
// Simple notifications
ImGui::AddSuccessNotification("File saved successfully");
ImGui::AddWarningNotification("This is a warning");
ImGui::AddErrorNotification("An error occurred");
ImGui::AddInfoNotification("Some information");

// With custom title
ImGui::AddNotification(ImGui::NotificationType_Success, "Custom Title", "Content here");

// With custom duration (milliseconds)
ImGui::AddSuccessNotification("Quick message", 1000);  // 1 second
ImGui::AddErrorNotification("Longer error", 8000);     // 8 seconds
```

### Configuration

```cpp
// Customize notification behavior
ImGui::NotificationConfig& cfg = ImGui::GetNotificationConfig();
cfg.width = 400.0f;           // Notification width (default: 320)
cfg.padding = 20.0f;         // Screen padding (default: 16)
cfg.spacing = 12.0f;         // Between notifications (default: 8)
cfg.border_radius = 8.0f;    // Corner radius (default: 6)
cfg.fade_duration_ms = 500;   // Fade time (default: 300)
cfg.max_visible = 3;         // Max concurrent (default: 5)
cfg.show_dismiss = true;     // Show X button (default: true)
```

### Rendering

Call `ImGui::RenderNotifications()` at the end of your render loop (after all other UI):

```cpp
// In your render loop
ImGui::RenderNotifications();
```

## Integration in pcode-editor

1. Header included at top of `editor_app.cpp`
2. Rendering called at end of `render()` function (line ~3284)
3. Example: Save notifications in `save_tab()` function (lines 751, 760)

## Color Scheme (Dark Theme)

| Type    | Background | Border | Default Duration |
|---------|------------|--------|------------------|
| Success | #1F5A2B    | #40A64D | 3000ms          |
| Warning | #735C1C    | #BF9933 | 4000ms          |
| Error   | #6B2424    | #BF4040 | 5000ms          |
| Info    | #243D6B    | #4D73BF | 3000ms          |

## Implementation Details

- **Header-only**: Just include `imgui_notify.h`
- **No external dependencies** beyond Dear ImGui
- **Uses `std::chrono`** for timing
- **Non-blocking**: Doesn't interrupt user input
- **Thread-safe cleanup**: Vector operations deferred to avoid iterator invalidation
- **ImGui best practices**: Uses unique window names, proper style push/pop

### Architecture

```
Notification struct
├── type (NotificationType)
├── title (std::string)
├── content (std::string)
├── start_time (steady_clock::time_point)
├── duration_ms (int)
├── dismissed (bool)
└── Methods: isExpired(), getAlpha()

NotificationConfig (global)
├── width, padding, spacing
├── border_radius, fade_duration_ms
├── max_visible, show_dismiss

Functions
├── AddNotification() - add to queue
├── AddSuccessNotification() - convenience
├── RenderNotifications() - render all
└── ClearNotifications() - clear queue
```

## Files

- `src/imgui_notify.h` - Main notification header (192 lines)
- `src/editor_app.cpp` - Integration example

## Changelog

### v2.0 (Current)
- ✅ Theme-aware colors matching pcode-editor dark theme
- ✅ Click-to-dismiss button
- ✅ Dynamic height calculation
- ✅ Max visible notifications limit
- ✅ Configurable via NotificationConfig
- ✅ Fixed vector iteration bug
- ✅ Shorter fade duration (300ms)
- ✅ Border styling
