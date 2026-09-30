# Worklog
Subset of total work, meant to leave a record

### 9-30-26: 
Transfer from mingw64 - ucrt64
Installed OpenCV 5.0.0 via pacman (`mingw-w64-ucrt-x86_64-opencv`); pkg-config name is `opencv5` (not `opencv4`), headers in `/ucrt64/include/opencv5`
Gotcha: hello.exe exited silently with code 127 (DLL not found). `ntldd -R` showed missing Qt6 DLLs (Qt6Core, Qt6Gui, Qt6Widgets, ...)
- HighGUI (`imshow`) in MSYS2's OpenCV 5 is built on Qt6, so Qt is required at runtime even though pacman lists it as optional
- Fix: `pacman -S mingw-w64-ucrt-x86_64-qt6-5compat` (pulls in qt6-base)
- Debugging note: `ldd`/`ntldd` report `api-ms-*` / `ext-ms-*` DLLs as "not found" -- these are Windows API-set aliases, not real missing files
hello_opencv test program builds and runs from the UCRT64 terminal
