# Developer Notifications System - Documentation

## Overview

Comprehensive notification system for pcode-editor development workflows, coordinating with the base `imgui_notify.h` toast system. Designed for IDE/editor integration with machine-readable payloads.

---

## Deep Dive Analysis

### Applicable Notification Types

| # | Type | Applicable? | Rationale |
|---|------|-------------|-----------|
| 1 | Build failure | ✅ Yes | CI and local CMake/MSBuild can fail |
| 2 | Tests failing | ✅ Yes | Regression tests exist |
| 3 | CI status change | ✅ Yes | GitHub Actions on PRs |
| 4 | Code review request | ✅ Yes | GitHub PR integration |
| 5 | Runtime error | ❌ No | Desktop app, not a service |
| 6 | Deployment | ⚠️ Optional | Could track releases |
| 7 | Security alerts | ✅ Yes | Secret scanning |
| 8 | Performance regressions | ⚠️ Limited | Desktop context |
| 9 | Flaky test detection | ✅ Yes | Test tracking |
| 10 | Dependency vulnerability | ✅ Yes | Advisory scanning |
| 11 | Long-running task | ✅ Yes | Build times |
| 12 | Workspace events | ✅ Yes | Git events |

### External Libraries Compared

| Feature | imgui-notify (external) | pcode-editor (custom) |
|---------|-------------------------|----------------------|
| Toast rendering | ✅ | ✅ |
| Multiple types | 4 | 4 + 9 payloads |
| Machine-readable | ❌ | ✅ |
| Action callbacks | ⚠️ | ✅ |
| Filtering | ❌ | ✅ |
| Quiet hours | ❌ | ✅ |

---

## Design Rules Applied

### Each Notification Includes:

1. **One-line summary (subject)** - Immediate context
2. **1-3 contextual fields** - Who/where/when
3. **Explicit action** - "Open log", "Re-run tests", etc.
4. **Opaque identifier** - Build ID, PR#, request ID
5. **Direct link** - URL to detailed view
6. **Sanitized snippet** - Short, safe excerpt

### Example Formats

```
Build fail: "Build failed — pcode-editor (target: all) — error: undefined at src/util.cpp:128 — Open log [ID#1234]"
Test fail:  "Tests broken — auth-suite — 3 fails — test_login: expected 200 got 500 — Re-run / View"
CI status:  "CI — PR#123 — failed — build step — by @author — Open PR"
```

---

## Notification Payloads

### 1. BuildFailurePayload

| Field | Type | Description |
|-------|------|-------------|
| `project` | string | Project name |
| `target` | string | Build target |
| `first_error_file` | string | File with error |
| `first_error_line` | int | Line number |
| `first_error_message` | string | Error message |
| `build_id` | string | Opaque build ID |
| `log_link` | string | URL to build log |

**Example:**
```cpp
notify_build_failure(
    "pcode-editor",           // project
    "Release",                // target  
    "src/util.cpp",           // error file
    128,                      // line
    "undefined reference",    // message
    "build_1234",             // ID
    "https://ci.logs/1234"    // link
);
```

### 2. TestsFailurePayload

| Field | Type | Description |
|-------|------|-------------|
| `suite_name` | string | Test suite |
| `failure_count` | int | Number failing |
| `top_failure_test` | string | First failing test |
| `top_failure_summary` | string | Failure reason |
| `is_flaky` | bool | Could be flaky |

### 3. CIStatusPayload

| Field | Type | Description |
|-------|------|-------------|
| `pr_branch` | string | PR or branch name |
| `previous_status` | string | Old status |
| `new_status` | string | New status |
| `failing_step` | string | Failed CI step |
| `author` | string | Triggered by |

### 4. ReviewRequestPayload

| Field | Type | Description |
|-------|------|-------------|
| `pr_title` | string | PR title |
| `files_changed` | int | Files modified |
| `labels` | vector<string> | PR labels |
| `is_draft` | bool | Draft PR |

### 5. SecurityAlertPayload

| Field | Type | Description |
|-------|------|-------------|
| `file_path` | string | Affected file |
| `severity_level` | string | CRITICAL/HIGH/MEDIUM |
| `affected_versions` | string | Vulnerable versions |
| `suggested_fix` | string | Fix version |
| `advisory_link` | string | Security advisory |

### 6. DependencyVulnPayload

| Field | Type | Description |
|-------|------|-------------|
| `package_name` | string | NPM/pkg name |
| `severity_level` | string | Severity |
| `affected_versions` | string | Bad versions |
| `suggested_version` | string | Fixed version |
| `advisory_id` | string | CVE ID |

### 7. TaskCompletePayload

| Field | Type | Description |
|-------|------|-------------|
| `task_name` | string | Task identifier |
| `task_result` | string | "success" or "failed" |
| `duration_seconds` | int | Runtime |
| `artifacts_link` | string | Build artifacts |
| `log_link` | string | Task log |

### 8. WorkspaceEventPayload

| Field | Type | Description |
|-------|------|-------------|
| `branch_name` | string | Git branch |
| `event_type` | string | force-push, deleted, etc |
| `affected_path` | string | File or repo path |
| `file_size_bytes` | size_t | File size if applicable |
| `author` | string | Who triggered |

### 9. PerfRegressionPayload

| Field | Type | Description |
|-------|------|-------------|
| `metric_name` | string | What was measured |
| `change_percent` | float | % change |
| `baseline_value` | float | Previous value |
| `current_value` | float | New value |
| `affected_area` | string | Editor area |

### 10. FlakyTestPayload

| Field | Type | Description |
|-------|------|-------------|
| `test_name` | string | Test name |
| `pass_count` | int | Recent passes |
| `fail_count` | int | Recent failures |
| `last_status` | string | Last result |

---

## Delivery Modes

| Mode | Use Case | Example |
|------|----------|---------|
| `Toast` | Immediate attention | Build fail, test fail |
| `Badge` | Status indicator | Perf regression, flaky test |
| `Inline` | In-editor display | Warnings |
| `Digest` | Batched summary | End of day |
| `Silent` | Logging only | Debug traces |

### Severity Mapping

| Severity | Default Duration | Color |
|----------|-----------------|-------|
| Critical | 8000ms | Red |
| High | 5000ms | Red |
| Medium | 4000ms | Yellow |
| Low | 2500ms | Green |

---

## Filtering & Configuration

### Per-Category Enable/Disable

```cpp
auto& mgr = EditorNotifications::NotificationManager::get();
mgr.set_filter(EditorNotifications::Category::Build, true);
mgr.set_filter(EditorNotifications::Category::Review, false);  // Mute PR notifications
```

### Quiet Hours

```cpp
// No notifications between 10 PM and 7 AM
mgr.set_quiet_hours(22, 7);
```

---

## Integration

### Include Header

```cpp
#include "editor_notifications.h"
```

### Using Convenience Functions

```cpp
// Build failure
notify_build_failure("pcode-editor", "Release", "src/main.cpp", 42, 
    "undefined reference", "build_123", "");

// Tests failing
notify_tests_failure("regression", 3, "test_save_file", 
    "expected true got false", "https://test.logs/123");

// CI status
notify_ci_status("PR#123", "pending", "failed", "build", 
    "@username", "https://github.com/...");

// Review request  
notify_review_request("Fix vim mode bug", "@author", 5, 
    {"bug", "high-priority"}, "123", "https://github.com/...", false);

// Security alert
notify_security_alert("src/keys.cpp", "HIGH", "1.0.0 - 1.2.0", 
    "1.2.1", "https://security.io/adv/123");

// Long task complete
notify_task_complete("CMake Build", true, 145, 
    "https://artifacts.com/build.zip", "");

// Git event
notify_git_event("main", "force-pushed", "", 0, "@author", "");
```

### Rendering

The notification system coordinates with `imgui_notify.h`:

```cpp
// In render loop - this renders both base toasts AND developer notifications
ImGui::RenderNotifications();
```

---

## Files

- `src/editor_notifications.h` - Main notification system (~600 lines)
- `src/imgui_notify.h` - Base toast rendering
- `src/editor_app.cpp` - Integration point

---

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    editor_notifications.h                   │
├─────────────────────────────────────────────────────────────┤
│  Notification Payloads (10 types)                           │
│  ├── BuildFailurePayload                                    │
│  ├── TestsFailurePayload                                    │
│  ├── CIStatusPayload                                        │
│  ├── ReviewRequestPayload                                   │
│  ├── SecurityAlertPayload                                   │
│  ├── DependencyVulnPayload                                  │
│  ├── TaskCompletePayload                                    │
│  ├── WorkspaceEventPayload                                  │
│  ├── PerfRegressionPayload                                  │
│  └── FlakyTestPayload                                       │
├─────────────────────────────────────────────────────────────┤
│  NotificationManager                                        │
│  ├── queue() - dispatch to appropriate toast               │
│  ├── set_filter() - category enable/disable                │
│  └── set_quiet_hours() - time-based filtering             │
├─────────────────────────────────────────────────────────────┤
│  Convenience Templates (notify_*)                           │
│  └── One-call notification helpers                          │
└─────────────────────────────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────┐
│                       imgui_notify.h                         │
├─────────────────────────────────────────────────────────────┤
│  Toast Rendering (Success/Warning/Error/Info)               │
│  RenderNotifications() - called at end of render loop     │
└─────────────────────────────────────────────────────────────┘
```

---

## Changelog

### v1.0 (Current)
- ✅ 10 machine-readable notification payloads
- ✅ Coordinated delivery with imgui_notify.h
- ✅ Severity-based duration and coloring
- ✅ Category filtering
- ✅ Quiet hours support
- ✅ Action callbacks (optional)
- ✅ Link/snippet support for quick context
