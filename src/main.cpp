// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 pCode Editor Development Team
// Author: Dennis O. Esternon <djwisdom@serenityos.org>
// Contributors: see CONTRIBUTORS or git history

// ============================================================================
// pcode-editor — Personal Code Editor
// Dear ImGui + GLFW + ImGuiColorTextEdit
// ============================================================================

#include "editor_app.h"

int main(int argc, char* argv[]) {
    EditorApp app(argc, argv);
    return app.run();
}
