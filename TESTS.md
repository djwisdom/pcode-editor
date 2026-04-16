# pcode-editor Test Suite

## Test 1: Build Test
```bash
cd build
cmake --build . --config Release
```
**Expected**: Build completes without errors

## Test 2: Menu Functionality
1. Run the app: `build/Release/pcode-editor.exe`
2. Click on "File" menu in menu bar
3. Click on "Edit" menu
4. Click on "View" menu
**Expected**: Menus should open and show menu items

## Test 3: Features Test
After confirming menus work, test these features:

| Feature | Test Steps | Expected |
|---------|------------|----------|
| Line Numbers | View menu → toggle Line Numbers | Line numbers show/hide |
| Whitespace | View menu → toggle Whitespace | Space dots show/hide |
| Code Folding | View menu → toggle Code Folding | Toggle enabled |
| Bookmarks | View menu → toggle Bookmarks | Toggle visible |
| Change Markers | View menu → toggle Change Markers | Toggle visible |
| Theme | View menu → toggle Dark/Light Theme | Colors change |
| Zoom | Ctrl++ / Ctrl+- / Ctrl+0 | Font size changes |
| Code Folding Hotkey | Ctrl+Shift+O on line with `{` | Line folds |
| Unfold Hotkey | Ctrl+Shift+[ | Line unfolds |

## Test 4: File Operations
1. File → Open → select a .cpp file
2. File → Save (Ctrl+S)
3. File → Save As → different name
4. Close tab with X button

## Test 5: Editor Operations
1. Type some text
2. Ctrl+F to find
3. Ctrl+H to replace
4. Ctrl+G to go to line