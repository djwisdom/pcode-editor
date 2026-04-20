#pragma once

// ============================================================================
// pcode-editor Developer Notifications
// ============================================================================
// Coordinated notification system for development workflows
// Integrates with imgui_notify.h for toast rendering
// ============================================================================
// Design rules:
// - One-line summary (subject) + contextual fields + explicit action
// - Machine-readable payload for filtering and actions
// - Severity-aware delivery (immediate toast vs badge vs digest)
// - Opaque identifiers with direct links
// ============================================================================

#include <string>
#include <vector>
#include <chrono>
#include <functional>
#include <optional>
#include <ctime>

#include "imgui_notify.h"

namespace EditorNotifications {

// Forward declare from imgui_notify.h
enum NotificationType {
    NotificationType_Success,
    NotificationType_Warning,
    NotificationType_Error,
    NotificationType_Info
};

void AddSuccessNotification(const char* content, int duration_ms = 3000);
void AddWarningNotification(const char* content, int duration_ms = 4000);
void AddErrorNotification(const char* content, int duration_ms = 5000);
void AddInfoNotification(const char* content, int duration_ms = 3000);

// ============================================================================
// Notification Categories (by urgency/severity)
// ============================================================================

enum class Severity {
    Critical,   // Immediate toast - developer should interrupt
    High,      // Immediate toast - review soon  
    Medium,    // Badge/inline indicator
    Low        // Digest or ignore
};

enum class Delivery {
    Toast,      // Immediate visible notification
    Badge,      // Editor indicator (gutter/status bar)
    Inline,     // Inline with content
    Digest,     // End-of-day summary
    Silent      // No notification, log only
};

enum class Category {
    Build,
    Tests,
    CI,
    Review,
    Security,
    Performance,
    Dependency,
    Task,
    Workspace
};

// ============================================================================
// Machine-Readable Payload Base
// ============================================================================

struct NotificationPayload {
    // Core fields
    Category category;
    Severity severity;
    Delivery delivery;
    std::string id;                    // Opaque identifier (build #, PR #, etc.)
    std::string link;                    // Direct URL to detailed view
    
    // Contextual fields
    std::string summary;                 // One-line subject
    std::string details;                // Additional context
    std::string location;              // file:line or endpoint
    std::string author;                // Who triggered
    
    // Timestamps
    std::chrono::system_clock::time_point timestamp;
    
    // Action
    std::string action_label;        // e.g., "Open log", "Rerun tests"
    std::function<void()> action;    // Callback (optional)
    
    // Snippet (sanitized, short)
    std::string snippet;
    
    NotificationPayload()
        : category(Category::Task), severity(Severity::Medium), delivery(Delivery::Toast),
          timestamp(std::chrono::system_clock::now()) {}
};

// ============================================================================
// Notification Factories (for each applicable type)
// ============================================================================

// ----------------------------------------------------------------------------
// 1. BUILD FAILURE
// ----------------------------------------------------------------------------
// "Build failed — project/name (target: target) — error: <snippet> — Open log [ID#1234]"
// ----------------------------------------------------------------------------

struct BuildFailurePayload : NotificationPayload {
    std::string project;
    std::string target;
    std::string first_error_file;
    int first_error_line;
    std::string first_error_message;
    
    static constexpr const char* CATEGORY_NAME = "build";
    static constexpr const char* DEFAULT_ACTION = "Open log";
    
    BuildFailurePayload(
        const std::string& project_name,
        const std::string& build_target,
        const std::string& error_file,
        int error_line,
        const std::string& error_message,
        const std::string& build_id,
        const std::string& log_link
    ) {
        category = Category::Build;
        severity = Severity::Critical;
        delivery = Delivery::Toast;
        id = build_id;
        link = log_link;
        project = project_name;
        target = build_target;
        first_error_file = error_file;
        first_error_line = error_line;
        first_error_message = error_message;
        
        summary = "Build failed — " + project_name + " (target: " + build_target + ")";
        details = "error: " + error_message;
        location = error_file + ":" + std::to_string(error_line);
        action_label = DEFAULT_ACTION;
        snippet = error_message;
    }
};

// ----------------------------------------------------------------------------
// 2. TESTS FAILING
// ----------------------------------------------------------------------------
// "Tests broken — suite/name — N fails — test_name: <summary> — Re-run / View"
// ----------------------------------------------------------------------------

struct TestsFailurePayload : NotificationPayload {
    std::string suite_name;
    int failure_count;
    std::string top_failure_test;
    std::string top_failure_summary;
    bool is_flaky;
    
    static constexpr const char* CATEGORY_NAME = "tests";
    static constexpr const char* DEFAULT_ACTION = "Re-run tests";
    
    TestsFailurePayload(
        const std::string& suite,
        int fail_count,
        const std::string& top_test,
        const std::string& failure_summary,
        bool flaky = false,
        const std::string& test_link = ""
    ) {
        category = Category::Tests;
        severity = flaky ? Severity::Medium : Severity::Critical;
        delivery = Delivery::Toast;
        id = suite + "_" + std::to_string(fail_count);
        link = test_link;
        suite_name = suite;
        failure_count = fail_count;
        top_failure_test = top_test;
        top_failure_summary = failure_summary;
        is_flaky = flaky;
        
        summary = "Tests broken — " + suite;
        details = std::to_string(fail_count) + (fail_count == 1 ? " fail" : " fails");
        action_label = flaky ? "View history" : DEFAULT_ACTION;
        snippet = top_test + ": " + failure_summary;
    }
};

// ----------------------------------------------------------------------------
// 3. CI STATUS CHANGE
// ----------------------------------------------------------------------------
// "CI — PR/branch — status — step — by author — Open PR [ID#1234]"
// ----------------------------------------------------------------------------

struct CIStatusPayload : NotificationPayload {
    std::string pr_branch;
    std::string previous_status;
    std::string new_status;
    std::string failing_step;
    bool is_merge_check;
    
    static constexpr const char* CATEGORY_NAME = "ci";
    static constexpr const char* DEFAULT_ACTION = "Open CI";
    
    CIStatusPayload(
        const std::string& pr_name,
        const std::string& prev_status,
        const std::string& current_status,
        const std::string& failing_step_name,
        const std::string& triggering_author,
        const std::string& ci_link,
        bool merge_check = false
    ) {
        category = Category::CI;
        severity = (current_status == "failed") ? Severity::Critical : Severity::High;
        delivery = Delivery::Toast;
        id = pr_name;
        link = ci_link;
        pr_branch = pr_name;
        previous_status = prev_status;
        new_status = current_status;
        failing_step = failing_step_name;
        is_merge_check = merge_check;
        author = triggering_author;
        
        summary = "CI — " + pr_name + " — " + current_status;
        details = failing_step_name.empty() ? "" : "failed step: " + failing_step_name;
        action_label = DEFAULT_ACTION;
        snippet = new_status;
    }
};

// ----------------------------------------------------------------------------
// 4. CODE REVIEW REQUEST
// ----------------------------------------------------------------------------
// "Review — PR title — by author — N files — labels — Open review [ID#1234]"
// ----------------------------------------------------------------------------

struct ReviewRequestPayload : NotificationPayload {
    std::string pr_title;
    int files_changed;
    std::vector<std::string> labels;
    bool is_draft;
    
    static constexpr const char* CATEGORY_NAME = "review";
    static constexpr const char* DEFAULT_ACTION = "Open review";
    
    ReviewRequestPayload(
        const std::string& title,
        const std::string& author_name,
        int file_count,
        const std::vector<std::string>& label_list,
        const std::string& pr_id,
        const std::string& pr_link,
        bool draft = false
    ) {
        category = Category::Review;
        severity = Severity::Medium;
        delivery = Delivery::Toast;
        id = pr_id;
        link = pr_link;
        pr_title = title;
        files_changed = file_count;
        labels = label_list;
        is_draft = draft;
        author = author_name;
        
        std::string label_str;
        if (!label_list.empty()) {
            label_str = " — [";
            for (size_t i = 0; i < label_list.size(); ++i) {
                if (i > 0) label_str += ", ";
                label_str += label_list[i];
            }
            label_str += "]";
        }
        
        summary = "Review — " + title;
        details = "by " + author_name + " — " + std::to_string(file_count) + " files" + label_str;
        action_label = DEFAULT_ACTION;
        snippet = std::to_string(file_count) + " files changed";
    }
};

// ----------------------------------------------------------------------------
// 5. SECURITY ALERT (NOT runtime - different from original request)
// ----------------------------------------------------------------------------
// "Security — repo/file — severity — <suggestion> — View report [ID#1234]"
// ----------------------------------------------------------------------------

struct SecurityAlertPayload : NotificationPayload {
    std::string file_path;
    std::string severity_level;      // CRITICAL, HIGH, MEDIUM, LOW
    std::string affected_versions;
    std::string suggested_fix;
    std::string advisory_link;
    
    static constexpr const char* CATEGORY_NAME = "security";
    static constexpr const char* DEFAULT_ACTION = "View report";
    
    SecurityAlertPayload(
        const std::string& path,
        const std::string& severity,
        const std::string& versions,
        const std::string& fix_version,
        const std::string& report_link,
        const std::string& adv_link = ""
    ) {
        category = Category::Security;
        this->severity = (severity == "CRITICAL" || severity == "HIGH") 
            ? Severity::Critical : Severity::High;
        delivery = Delivery::Toast;
        id = path;
        link = report_link;
        file_path = path;
        severity_level = severity;
        affected_versions = versions;
        suggested_fix = fix_version;
        advisory_link = adv_link;
        
        summary = "Security — " + path;
        details = "severity: " + severity + "; update to " + fix_version;
        action_label = DEFAULT_ACTION;
        snippet = severity + " in " + versions;
    }
};

// ----------------------------------------------------------------------------
// 6. DEPENDENCY VULNERABILITY
// ----------------------------------------------------------------------------
// "Vuln — package — severity — versions — fix: version — View advisory [ID#1234]"
// ----------------------------------------------------------------------------

struct DependencyVulnPayload : NotificationPayload {
    std::string package_name;
    std::string severity_level;
    std::string affected_versions;
    std::string suggested_version;
    std::string advisory_id;
    
    static constexpr const char* CATEGORY_NAME = "dependency";
    static constexpr const char* DEFAULT_ACTION = "View advisory";
    
    DependencyVulnPayload(
        const std::string& package,
        const std::string& severity,
        const std::string& versions,
        const std::string& fix_version,
        const std::string& adv_id,
        const std::string& adv_link
    ) {
        category = Category::Dependency;
        this->severity = (severity == "CRITICAL" || severity == "HIGH") 
            ? Severity::Critical : Severity::High;
        delivery = Delivery::Toast;
        id = adv_id;
        link = adv_link;
        package_name = package;
        severity_level = severity;
        affected_versions = versions;
        suggested_version = fix_version;
        advisory_id = adv_id;
        
        summary = "Vuln — " + package;
        details = severity + "; fix: " + fix_version;
        action_label = DEFAULT_ACTION;
        snippet = severity + " in " + versions;
    }
};

// ----------------------------------------------------------------------------
// 7. LONG-RUNNING TASK COMPLETED
// ----------------------------------------------------------------------------
// "Task done — task/name — duration — status — View artifacts [ID#1234]"
// ----------------------------------------------------------------------------

struct TaskCompletePayload : NotificationPayload {
    std::string task_name;
    std::string task_result;         // "success" or "failed"
    int duration_seconds;
    std::string artifacts_link;
    
    static constexpr const char* CATEGORY_NAME = "task";
    static constexpr const char* DEFAULT_ACTION = "View artifacts";
    static constexpr const char* DEFAULT_FAIL_ACTION = "View log";
    
    TaskCompletePayload(
        const std::string& task,
        bool success,
        int seconds,
        const std::string& art_link = "",
        const std::string& log_link = ""
    ) {
        category = Category::Task;
        severity = success ? Severity::Low : Severity::High;
        delivery = success ? Delivery::Toast : Delivery::Toast;
        id = task + "_" + std::to_string(seconds);
        link = success ? art_link : log_link;
        task_name = task;
        task_result = success ? "success" : "failed";
        duration_seconds = seconds;
        
        summary = "Task done — " + task;
        
        int mins = seconds / 60;
        int secs = seconds % 60;
        std::string dur = mins > 0 
            ? std::to_string(mins) + "m " + std::to_string(secs) + "s"
            : std::to_string(secs) + "s";
        
        details = "duration: " + dur + " — " + task_result;
        action_label = success ? DEFAULT_ACTION : DEFAULT_FAIL_ACTION;
        snippet = task_result + " in " + dur;
    }
};

// ----------------------------------------------------------------------------
// 8. WORKSPACE EVENTS
// ----------------------------------------------------------------------------
// "Git — branch — event — by author — time — View [ID#1234]"
// ----------------------------------------------------------------------------

struct WorkspaceEventPayload : NotificationPayload {
    std::string branch_name;
    std::string event_type;         // force-push, deleted, large-file, etc.
    std::string affected_path;
    size_t file_size_bytes;
    
    static constexpr const char* CATEGORY_NAME = "workspace";
    static constexpr const char* DEFAULT_ACTION = "View";
    
    WorkspaceEventPayload(
        const std::string& branch,
        const std::string& event,
        const std::string& path,
        size_t size_bytes,
        const std::string& author_name,
        const std::string& event_link = ""
    ) {
        category = Category::Workspace;
        severity = (event == "force-push" || event == "deleted") 
            ? Severity::High : Severity::Medium;
        delivery = Delivery::Toast;
        id = branch + "_" + event;
        link = event_link;
        branch_name = branch;
        event_type = event;
        affected_path = path;
        file_size_bytes = size_bytes;
        author = author_name;
        
        summary = "Git — " + branch + " — " + event;
        
        if (event == "large-file" && size_bytes > 0) {
            double mb = size_bytes / (1024.0 * 1024.0);
            details = "added: " + path + " (" + std::to_string((int)mb) + " MB)";
        } else {
            details = "by " + author_name;
        }
        
        action_label = DEFAULT_ACTION;
        snippet = event_type;
    }
};

// ----------------------------------------------------------------------------
// 9. PERFORMANCE REGRESSION (simplified for desktop app context)
// ----------------------------------------------------------------------------
// "Perf — metric — N% change — baseline vs current — <link>"
// ----------------------------------------------------------------------------

struct PerfRegressionPayload : NotificationPayload {
    std::string metric_name;
    float change_percent;
    float baseline_value;
    float current_value;
    std::string affected_area;
    
    static constexpr const char* CATEGORY_NAME = "performance";
    static constexpr const char* DEFAULT_ACTION = "View graph";
    
    PerfRegressionPayload(
        const std::string& metric,
        float pct_change,
        float baseline,
        float current,
        const std::string& area,
        const std::string& graph_link = ""
    ) {
        category = Category::Performance;
        severity = std::abs(pct_change) > 20.0f ? Severity::High : Severity::Medium;
        delivery = Delivery::Badge;
        id = metric;
        link = graph_link;
        metric_name = metric;
        change_percent = pct_change;
        baseline_value = baseline;
        current_value = current;
        affected_area = area;
        
        std::string sign = pct_change > 0 ? "+" : "";
        summary = "Perf — " + metric + " — " + sign + std::to_string((int)pct_change) + "%";
        details = "baseline: " + std::to_string((int)baseline) + " vs current: " + std::to_string((int)current);
        action_label = DEFAULT_ACTION;
        snippet = sign + std::to_string((int)pct_change) + "% " + area;
    }
};

// ----------------------------------------------------------------------------
// 10. FLAKY TEST DETECTION
// ----------------------------------------------------------------------------
// "Flaky — test/name — pass/fail history — View history [ID#1234]"
// ----------------------------------------------------------------------------

struct FlakyTestPayload : NotificationPayload {
    std::string test_name;
    int pass_count;
    int fail_count;
    std::string last_status;
    
    static constexpr const char* CATEGORY_NAME = "flaky";
    static constexpr const char* DEFAULT_ACTION = "View history";
    
    FlakyTestPayload(
        const std::string& test,
        int passes,
        int fails,
        const std::string& last_result,
        const std::string& history_link = ""
    ) {
        category = Category::Tests;
        severity = Severity::Medium;
        delivery = Delivery::Badge;
        id = test;
        link = history_link;
        test_name = test;
        pass_count = passes;
        fail_count = fails;
        last_status = last_result;
        
        summary = "Flaky — " + test;
        details = std::to_string(passes) + "/" + std::to_string(passes + fails) + " passes";
        action_label = DEFAULT_ACTION;
        snippet = last_result;
    }
};

// ============================================================================
// Notification Queue and Coordination
// ============================================================================

struct QueuedNotification {
    NotificationPayload base;
    std::string toast_title;      // What shows in toast title
    std::string toast_content;   // What shows in toast body
    
    QueuedNotification() {}
    
    template<typename Payload>
    QueuedNotification(Payload& p, const std::string& title, const std::string& content)
        : base(p), toast_title(title), toast_content(content) {}
};

class NotificationManager {
public:
    static NotificationManager& get() {
        static NotificationManager instance;
        return instance;
    }
    
    // Queue a notification for rendering
    template<typename Payload>
    void queue(const Payload& payload, int duration_ms = 3000) {
        // Map category/severity to toast type
        NotificationType toast_type = NotificationType_Info;
        
        switch (payload.severity) {
            case Severity::Critical:
            case Severity::High:
                toast_type = NotificationType_Error;
                break;
            case Severity::Medium:
                toast_type = NotificationType_Warning;
                break;
            case Severity::Low:
                toast_type = NotificationType_Success;
                break;
        }
        
        // Apply category overrides
        switch (payload.category) {
            case Category::Security:
            case Category::Dependency:
                toast_type = NotificationType_Error;
                break;
            case Category::Review:
                toast_type = NotificationType_Info;
                break;
            default:
                break;
        }
        
        // Generate toast content
        std::string toast_content = payload.summary;
        if (!payload.snippet.empty()) {
            toast_content += "\n" + payload.snippet;
        }
        
        // Add action hint if present
        if (!payload.action_label.empty()) {
            toast_content += "\n" + payload.action_label + (payload.link.empty() ? "" : " → " + payload.link);
        }
        
        // Use appropriate duration
        int adjusted_duration = duration_ms;
        switch (payload.severity) {
            case Severity::Critical:
                adjusted_duration = 8000;  // Longer for critical
                break;
            case Severity::High:
                adjusted_duration = 5000;
                break;
            case Severity::Medium:
                adjusted_duration = 4000;
                break;
            case Severity::Low:
                adjusted_duration = 2500;
                break;
        }
        
        // Add to queue
        switch (toast_type) {
            case NotificationType_Success:
                ImGui::AddSuccessNotification(toast_content.c_str(), adjusted_duration);
                break;
            case NotificationType_Warning:
                ImGui::AddWarningNotification(toast_content.c_str(), adjusted_duration);
                break;
            case NotificationType_Error:
                ImGui::AddErrorNotification(toast_content.c_str(), adjusted_duration);
                break;
            case NotificationType_Info:
            default:
                ImGui::AddInfoNotification(toast_content.c_str(), adjusted_duration);
                break;
        }
    }
    
    // Filter configuration
    void set_filter(Category cat, bool enabled) {
        filters_[static_cast<int>(cat)] = enabled;
    }
    
    bool is_enabled(Category cat) const {
        return filters_[static_cast<int>(cat)];
    }
    
    // Quiet hours
    void set_quiet_hours(int start_hour, int end_hour) {
        quiet_start_ = start_hour;
        quiet_end_ = end_hour;
    }
    
    bool is_quiet_hours() const {
        auto now = std::chrono::system_clock::now();
        std::tm* tm = std::localtime(reinterpret_cast<const std::time_t*>(&now));
        int hour = tm ? tm->tm_hour : 0;
        if (quiet_start_ <= quiet_end_) {
            return hour >= quiet_start_ && hour < quiet_end_;
        }
        return hour >= quiet_start_ || hour < quiet_end_;
    }
    
private:
    NotificationManager() {
        // Enable all by default
        for (auto& f : filters_) f = true;
    }
    
    bool filters_[static_cast<int>(Category::Workspace) + 1] = {true, true, true, true, true, true, true, true, true};
    int quiet_start_ = 0;
    int quiet_end_ = 0;
};

// ============================================================================
// Convenience Templates
// ============================================================================

inline void notify_build_failure(
    const std::string& project,
    const std::string& target,
    const std::string& error_file,
    int error_line,
    const std::string& error_msg,
    const std::string& build_id,
    const std::string& log_link
) {
    BuildFailurePayload p(project, target, error_file, error_line, error_msg, build_id, log_link);
    NotificationManager::get().queue(p);
}

inline void notify_tests_failure(
    const std::string& suite,
    int fail_count,
    const std::string& top_test,
    const std::string& summary,
    const std::string& link = ""
) {
    TestsFailurePayload p(suite, fail_count, top_test, summary, false, link);
    NotificationManager::get().queue(p);
}

inline void notify_flaky_test(
    const std::string& test,
    int passes,
    int fails,
    const std::string& last_result,
    const std::string& link = ""
) {
    FlakyTestPayload p(test, passes, fails, last_result, link);
    NotificationManager::get().queue(p);
}

inline void notify_ci_status(
    const std::string& pr,
    const std::string& prev_status,
    const std::string& new_status,
    const std::string& step,
    const std::string& author,
    const std::string& link
) {
    CIStatusPayload p(pr, prev_status, new_status, step, author, link);
    NotificationManager::get().queue(p);
}

inline void notify_review_request(
    const std::string& title,
    const std::string& author,
    int files,
    const std::vector<std::string>& labels,
    const std::string& pr_id,
    const std::string& link,
    bool draft = false
) {
    ReviewRequestPayload p(title, author, files, labels, pr_id, link, draft);
    NotificationManager::get().queue(p);
}

inline void notify_security_alert(
    const std::string& file,
    const std::string& severity,
    const std::string& versions,
    const std::string& fix,
    const std::string& link
) {
    SecurityAlertPayload p(file, severity, versions, fix, link);
    NotificationManager::get().queue(p);
}

inline void notify_dependency_vuln(
    const std::string& pkg,
    const std::string& severity,
    const std::string& versions,
    const std::string& fix,
    const std::string& adv_id,
    const std::string& link
) {
    DependencyVulnPayload p(pkg, severity, versions, fix, adv_id, link);
    NotificationManager::get().queue(p);
}

inline void notify_task_complete(
    const std::string& task,
    bool success,
    int seconds,
    const std::string& artifacts = "",
    const std::string& log = ""
) {
    TaskCompletePayload p(task, success, seconds, artifacts, log);
    NotificationManager::get().queue(p);
}

inline void notify_git_event(
    const std::string& branch,
    const std::string& event,
    const std::string& path,
    size_t size,
    const std::string& author,
    const std::string& link = ""
) {
    WorkspaceEventPayload p(branch, event, path, size, author, link);
    NotificationManager::get().queue(p);
}

inline void notify_perf_regression(
    const std::string& metric,
    float pct,
    float baseline,
    float current,
    const std::string& area,
    const std::string& link = ""
) {
    PerfRegressionPayload p(metric, pct, baseline, current, area, link);
    NotificationManager::get().queue(p);
}

} // namespace EditorNotifications