# pcode-editor FAQ

Frequently asked questions and common issues encountered while building and using pcode-editor.

## Version

This FAQ covers **pcode-editor version 0.2.28**.

---

## Build Issues

### CMake: "Could not find CMake configuration files"

**Symptoms:**
```
Could not find a package configuration file named "GLFWConfig.cmake"
```

**Cause:** GLFW not found in system paths.

**Solution:**
```bash
# Using build scripts handles this automatically
./scripts/build.sh Release
# Or specify FetchContent
cmake .. -DCMAKE_TOOLCHAIN_FILE=cmake/fetch_deps.cmake
```

---

### MSVC: "Access denied" when building

**Symptoms:**
```
Access denied to path: build-windows/Release/pcode-editor.exe
```

**Cause:** Executable is still running.

**Solution:**
1. Close the running editor
2. Or use Task Manager to kill the process

```cmd
taskkill /F /IM pcode-editor.exe
```

---

### MSVC: "Cannot open include file: 'GLFW/glfw3.h'"

**Symptoms:**
```
fatal error: GLFW/glfw3.h: No such file or directory
```

**Cause:** Dependencies not fetched.

**Solution:**
```cmd
# Clean and rebuild
rmdir /s /q build-windows
scripts\build-windows.bat Release
```

---

### Linux: "Undefined reference to 'glfwInit'"

**Symptoms:**
```
undefined reference to `glfwInit'
```

**Cause:** GLFW library not linked.

**Solution:**
```bash
# Ensure build-linux.sh is used (not manual cmake)
./scripts/build-linux.sh Release
```

---

### Linux: Wayland not working / Black screen

**Symptoms:** Window opens but nothing renders.

**Cause:** Running on X11 session instead of Wayland.

**Solution:**
```bash
# Set WAYLAND_DISPLAY
export WAYLAND_DISPLAY=wayland-0

# Or build with X11
cmake .. -DGLFW_BUILD_WAYLAND=OFF -DGLFW_BUILD_X11=ON
make -j$(nproc)
```

---

### FreeBSD: "libgtk-3.so: cannot open shared object file"

**Symptoms:**
```
libgtk-3.so: cannot open shared object file: No such file or directory
```

**Cause:** GTK3 not installed.

**Solution:**
```bash
# FreeBSD
sudo pkg install gtk3

# Install other dependencies
sudo pkg install cmake mesa-libs pkgconf
```

---

### macOS: "ld: library not found for -lglfw"

**Symptoms:**
```
ld: library not found for -lglfw
```

**Cause:** Homebrew not properly initialized.

**Solution:**
```bash
# Add Homebrew to path
export PATH=/opt/homebrew/bin:$PATH

# Or reinstall glfw
brew reinstall glfw
```

---

### Build script fails on Windows PowerShell

**Symptoms:**
```
ExecutionPolicy restriction
```

**Cause:** PowerShell execution policy blocks scripts.

**Solution:**
```powershell
# Set execution policy for this session
Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope Process

# Then run the build
.\scripts\build-windows.bat Release
```

---

## Runtime Issues

### Editor crashes on startup

**Cause 1:** Corrupted settings file.

**Solution:** Delete `pcode-settings.json`:
```cmd
del pcode-settings.json
```
The editor will create a new one with defaults.

**Cause 2:** Missing font file.

**Solution:** Set font_name to empty in settings:
```json
"font_name": ""
```

---

### Fonts too small on high DPI displays

**Issue:** Text appears tiny on 4K/HiDPI screens.

**Solution:** Edit `pcode-settings.json`:
```json
"font_size": 18
```
The font_size is automatically scaled for DPI. For 125% DPI, setting 18 gives ~22px effective size.

---

### Terminal not responding to keyboard input

**Issue:** Cannot type in terminal window.

**Solution:** Click on the terminal window first to focus it. The terminal captures keyboard input when focused.

---

### Files not saving

**Cause 1:** Read-only file system.

**Solution:** Check file permissions:
```cmd
attrib filename
```
Remove read-only attribute:
```cmd
attrib -R filename
```

**Cause 2:** No disk space.

**Solution:** Free up disk space.

---

### Vim mode not working

**Cause:** Accidental key press changed mode.

**Solution:** Press `Esc` to return to NORMAL mode, then press `i` for INSERT mode.

---

### Split windows not creating

**Issue:** :sp or :vsp command does nothing.

**Solution:** Ensure the command has a colon (ex command):
```
:sp
```
Not just `sp`.

---

### Minimap not showing

**Solution:** Toggle in settings or via command palette:
```
Ctrl+P
# Type "toggle minimap"
```

---

### Recent files list is empty

**Cause:** Settings file was reset or `recent_files` was cleared.

**Solution:** Open a few files. They will be automatically added to the list (max 10).

---

### Cannot open large files

**Cause:** Memory limitations.

**Solution:** No direct fix. Consider splitting large files or using alternative editors for files >10MB.

---

### Theme not switching

**Cause:** Theme toggle requires reload.

**Solution:**
```
:set theme
```
Or toggle via View menu.

---

## Platform-Specific Issues

### Windows: OpenGL not available in VMware

**Issue:** No hardware 3D acceleration.

**Solution:** Enable 3D acceleration in VMware settings, or use software renderer:
```cpp
// In editor_app.cpp, change:
glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
// to
glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 1);
```

---

### Linux: "libwayland-client.so.0 not found"

**Solution:**
```bash
# Ubuntu/Debian
sudo apt install libwayland-client0

# Fedora
sudo dnf install wayland-libs
```

---

### WSL2: Cannot display GUI

**Solution:** Use X11 forwarding:
```bash
# Install VcXsrv on Windows
# In WSL:
export DISPLAY=$(grep nameserver /etc/resolv.conf | awk '{print $2}'):0
```

---

## Configuration Issues

### Settings not persisting

**Cause:** File permissions or format error.

**Solution:** Check `pcode-settings.json` is valid JSON:
```bash
# Validate (using jq)
cat pcode-settings.json | jq .
```

---

### Font not loading

**Issue:** Editor uses wrong or no font.

**Solution:** In settings, use empty font_name for default:
```json
"font_name": ""
```
Or use a font installed on the system:
```json
"font_name": "JetBrains Mono"
```

---

### Command palette not filtering

**Issue:** All commands show, filtering not working.

**Cause:** This is the expected behavior in current version. All commands are shown.

---

## Keyboard Shortcuts Not Working

### Cause 1: Wrong mode (Vim)

**Solution:** Press `Esc` to return to NORMAL mode.

### Cause 2: Another app has focus

**Solution:** Click on editor window.

### Cause 3: Key conflict with OS

**Solution:** Some shortcuts may conflict with system shortcuts. Try the alternative shortcuts listed in the userguide.

---

## Performance Issues

### Slow with large files

**Solution:**
1. Disable minimap (reduces rendering)
2. Close unused tabs
3. Reduce recent files list

### High CPU usage

**Cause:** Auto-save or syntax highlighting on large files.

**Solution:** Reduce auto-save interval or disable in source code:
```cpp
// In editor_app.h
int auto_save_interval = 30;  // Increase to disable
```

---

## Getting Help

### Building from source

1. Check the README.md for platform-specific instructions
2. Verify all dependencies are installed
3. Use the provided build scripts

### Reporting bugs

1. Check if issue is in this FAQ
2. Try clean build
3. Check the GitHub Issues page

---

## License

BSD 2-Clause License — see LICENSE file.