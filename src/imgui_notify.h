#pragma once

// ============================================================================
// pcode-editor Notification System
// ============================================================================
// A simplified toast notification system for Dear ImGui
// Features: Success, Warning, Error, Info types with auto-dismiss
// ============================================================================

#include <imgui.h>
#include <string>
#include <vector>
#include <chrono>
#include <algorithm>

namespace ImGui {

// Notification types
enum NotificationType {
    NotificationType_Success,
    NotificationType_Warning,
    NotificationType_Error,
    NotificationType_Info
};

// Notification configuration
struct NotificationConfig {
    float width = 320.0f;
    float padding = 16.0f;
    float spacing = 8.0f;
    float border_radius = 6.0f;
    int fade_duration_ms = 300;
    int max_visible = 5;
    bool show_dismiss = true;
};

// Get global config (customizable)
inline NotificationConfig& GetNotificationConfig() {
    static NotificationConfig config;
    return config;
}

// Detect if currently in dark theme (based on WindowBg)
inline bool IsDarkTheme() {
    ImVec4 bg = ImGui::GetStyle().Colors[ImGuiCol_WindowBg];
    return bg.x < 0.5f;  // Dark if bg luminance < 50%
}

// Notification colors - adapt to dark/light theme
inline ImVec4 GetNotificationBgColor(NotificationType type) {
    bool dark = IsDarkTheme();
    switch (type) {
        case NotificationType_Success: 
            return dark ? ImVec4(0.12f, 0.35f, 0.17f, 0.95f) : ImVec4(0.70f, 0.90f, 0.75f, 0.95f);
        case NotificationType_Warning:  
            return dark ? ImVec4(0.45f, 0.36f, 0.11f, 0.95f) : ImVec4(0.95f, 0.90f, 0.60f, 0.95f);
        case NotificationType_Error:    
            return dark ? ImVec4(0.42f, 0.14f, 0.14f, 0.95f) : ImVec4(0.95f, 0.75f, 0.75f, 0.95f);
        case NotificationType_Info:      
            return dark ? ImVec4(0.14f, 0.24f, 0.42f, 0.95f) : ImVec4(0.75f, 0.85f, 0.95f, 0.95f);
        default: 
            return dark ? ImVec4(0.20f, 0.20f, 0.22f, 0.95f) : ImVec4(0.85f, 0.85f, 0.88f, 0.95f);
    }
}

inline ImVec4 GetNotificationBorderColor(NotificationType type) {
    bool dark = IsDarkTheme();
    switch (type) {
        case NotificationType_Success: 
            return dark ? ImVec4(0.25f, 0.65f, 0.30f, 0.60f) : ImVec4(0.20f, 0.55f, 0.15f, 0.60f);
        case NotificationType_Warning:  
            return dark ? ImVec4(0.75f, 0.60f, 0.20f, 0.60f) : ImVec4(0.55f, 0.45f, 0.10f, 0.60f);
        case NotificationType_Error:    
            return dark ? ImVec4(0.75f, 0.25f, 0.25f, 0.60f) : ImVec4(0.55f, 0.15f, 0.15f, 0.60f);
        case NotificationType_Info:      
            return dark ? ImVec4(0.30f, 0.45f, 0.75f, 0.60f) : ImVec4(0.20f, 0.35f, 0.60f, 0.60f);
        default: 
            return dark ? ImVec4(0.40f, 0.40f, 0.45f, 0.60f) : ImVec4(0.30f, 0.30f, 0.35f, 0.60f);
    }
}

// Default titles for each type
inline const char* GetDefaultTitle(NotificationType type) {
    switch (type) {
        case NotificationType_Success: return "Success";
        case NotificationType_Warning:  return "Warning";
        case NotificationType_Error:   return "Error";
        case NotificationType_Info:    return "Info";
        default: return "Notification";
    }
}

// Internal notification data
struct Notification {
    NotificationType type;
    std::string title;
    std::string content;
    std::chrono::steady_clock::time_point start_time;
    int duration_ms;
    bool dismissed;

    Notification(NotificationType type, const char* title, const char* content, int duration_ms)
        : type(type), title(title), content(content), duration_ms(duration_ms), dismissed(false) {
        start_time = std::chrono::steady_clock::now();
    }

    bool isExpired() const {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start_time).count();
        return elapsed >= duration_ms || dismissed;
    }

    float getAlpha() const {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start_time).count();
        int fade_time = GetNotificationConfig().fade_duration_ms;
        if (elapsed > duration_ms - fade_time && elapsed < duration_ms) {
            return (float)(duration_ms - elapsed) / (float)fade_time;
        }
        return 1.0f;
    }
};

// Notification container
static std::vector<Notification> g_notifications;

// Add a notification to the queue
inline void AddNotification(NotificationType type, const char* content, int duration_ms = 3000) {
    int max_visible = GetNotificationConfig().max_visible;
    if ((int)g_notifications.size() >= max_visible) {
        return;
    }
    g_notifications.emplace_back(type, GetDefaultTitle(type), content, duration_ms);
}

// Add a notification with custom title
inline void AddNotification(NotificationType type, const char* title, const char* content, int duration_ms = 3000) {
    int max_visible = GetNotificationConfig().max_visible;
    if ((int)g_notifications.size() >= max_visible) {
        return;
    }
    g_notifications.emplace_back(type, title, content, duration_ms);
}

// Convenience functions
inline void AddSuccessNotification(const char* content, int duration_ms = 3000) {
    AddNotification(NotificationType_Success, content, duration_ms);
}

inline void AddWarningNotification(const char* content, int duration_ms = 4000) {
    AddNotification(NotificationType_Warning, content, duration_ms);
}

inline void AddErrorNotification(const char* content, int duration_ms = 5000) {
    AddNotification(NotificationType_Error, content, duration_ms);
}

inline void AddInfoNotification(const char* content, int duration_ms = 3000) {
    AddNotification(NotificationType_Info, content, duration_ms);
}

// Render all notifications
inline void RenderNotifications() {
    if (g_notifications.empty()) return;

    ImGuiIO& io = ImGui::GetIO();
    ImVec2 viewport_size = io.DisplaySize;
    const NotificationConfig& cfg = GetNotificationConfig();

    std::vector<Notification> active_notifs;
    for (auto& n : g_notifications) {
        if (!n.isExpired()) {
            active_notifs.push_back(n);
        }
    }
    g_notifications.swap(active_notifs);

    if (g_notifications.empty()) return;

    float current_y = viewport_size.y - cfg.padding;
    for (size_t i = g_notifications.size(); i > 0; --i) {
        auto& notif = g_notifications[i - 1];
        if (notif.isExpired()) continue;

        float alpha = notif.getAlpha();
        if (alpha <= 0.0f) continue;

        ImVec2 pos(viewport_size.x - cfg.padding - cfg.width, current_y - 60.0f);
        ImVec2 size(cfg.width, 60.0f);

        ImVec4 bg_color = GetNotificationBgColor(notif.type);
        bg_color.w *= alpha;
        ImVec4 border_color = GetNotificationBorderColor(notif.type);
        border_color.w *= alpha;
        ImVec4 text_color = {1.0f, 1.0f, 1.0f, alpha};

        ImGui::PushStyleColor(ImGuiCol_WindowBg, bg_color);
        ImGui::PushStyleColor(ImGuiCol_Border, border_color);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, cfg.border_radius);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);

        ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
        ImGui::SetNextWindowSize(size, ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.0f);

        bool show = true;
        if (ImGui::Begin(("##Notify_" + std::to_string(i)).c_str(), &show,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs)) {

            ImGui::PushStyleColor(ImGuiCol_Text, text_color);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));

            float title_w = ImGui::CalcTextSize(notif.title.c_str()).x;
            float dismiss_w = cfg.show_dismiss ? 20.0f : 0.0f;
            float avail_w = cfg.width - 2 * cfg.padding - title_w - dismiss_w;

            ImGui::SetCursorPosX(cfg.padding);
            ImGui::Text(notif.title.c_str());

            if (cfg.show_dismiss) {
                ImGui::SameLine();
                ImGui::SetCursorPosX(cfg.width - cfg.padding - 16.0f);
                ImGui::TextColored(ImVec4(1.0f, 1.0f, 1.0f, 0.7f * alpha), "x");
                if (ImGui::IsItemClicked()) {
                    notif.dismissed = true;
                }
            }

            ImGui::SetCursorPosX(cfg.padding);
            ImGui::SetCursorPosY(cfg.padding + 18.0f);
            ImGui::PushTextWrapPos(cfg.padding + avail_w);
            ImGui::TextWrapped(notif.content.c_str());
            ImGui::PopTextWrapPos();

            ImGui::PopStyleVar(2);
            ImGui::PopStyleColor();
            ImGui::End();
            
            ImGui::PopStyleColor(2);
            ImGui::PopStyleVar(3);
        } else {
            // Begin failed - only pop the outer style vars
            ImGui::PopStyleColor(2);
            ImGui::PopStyleVar(3);
        }

        current_y -= 65.0f;
    }

    std::vector<Notification> remaining;
    for (auto& n : g_notifications) {
        if (!n.isExpired()) {
            remaining.push_back(n);
        }
    }
    g_notifications.swap(remaining);
}

// Clear all notifications
inline void ClearNotifications() {
    g_notifications.clear();
}

} // namespace ImGui