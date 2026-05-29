# JustDuck Translator

Windows-first C++ desktop MVP for hover/region translation.

## Current MVP

- Global hotkey: `Ctrl + Shift + T`.
- Fullscreen drag-to-select region picker.
- GDI screen capture for the selected rectangle.
- Local Windows OCR through `Windows.Media.Ocr`.
- Translation request through `translate.googleapis.com`.
- Always-on-top overlay near the selected region.
- Local settings file for target language.
- In-memory translation cache.

## Build

Install Visual Studio 2022 with the Desktop C++ workload and Windows SDK, then:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

Run:

```powershell
.\build\Release\JustDuckTranslator.exe
```

This workspace was also verified with a direct MSVC build, producing:

```powershell
.\build\manual\JustDuckTranslator.exe
```

The app starts in the tray. Press `Ctrl + Shift + T`, drag a screen region, and release.

## Notes

OCR stays local. Only extracted text is sent to the translation endpoint.

The default translation endpoint is compatible with the extension prototype but is not an official production API. Replace `TranslationClient` with Google Cloud Translation, Microsoft Translator, DeepL, or a private backend before public release.
