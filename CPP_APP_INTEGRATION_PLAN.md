# C++ Desktop App Integration Plan

Created by JustDuck.

This document is a blueprint for turning the Hover Translate idea into a Windows desktop app written in C++.

## Product Goal

Build a desktop app that lets users translate text from anywhere on screen, including images, PDFs, games, videos, browser pages, and native apps.

Main flow:

```text
User presses global hotkey
-> app captures a selected screen region
-> local OCR extracts text and text boxes
-> translation engine translates the text
-> app shows a floating overlay with the translated result
```

## Recommended Scope

Start Windows-first.

Do not train a custom model in the first version. Build the app pipeline first, then add custom translation later.

MVP:

- Global hotkey, for example `Ctrl + Shift + T`.
- Region selection screenshot.
- Local OCR.
- Translation through an API.
- Floating overlay above or near the selected region.
- Settings for target language and hotkey.

## Suggested Architecture

```text
JustDuck Translator
  app/
    main.cpp
    AppController
    SettingsStore
    HotkeyManager
    CaptureManager
    RegionSelector
    OcrEngine
    TranslationClient
    OverlayWindow
    HistoryStore
```

Responsibilities:

- `AppController`: coordinates the full translate workflow.
- `SettingsStore`: stores target language, hotkey, OCR language, API endpoint, and UI preferences.
- `HotkeyManager`: registers and listens for global hotkeys.
- `CaptureManager`: captures screen or selected region.
- `RegionSelector`: lets the user drag a rectangle on screen.
- `OcrEngine`: extracts text and bounding boxes from an image.
- `TranslationClient`: sends OCR text to a translation API and parses the response.
- `OverlayWindow`: renders translated text on top of the desktop.
- `HistoryStore`: optional local history for later correction and model training.

## Technology Choices

### UI

Recommended:

- WinUI 3 if you want modern Windows UI.
- Qt if you want future cross-platform support.
- Raw Win32/Direct2D if you want maximum control and minimum dependencies.

For MVP speed, use Qt or WinUI 3.

### Global Hotkey

Use Win32:

```cpp
RegisterHotKey(hwnd, HOTKEY_TRANSLATE, MOD_CONTROL | MOD_SHIFT, 'T');
```

Listen for:

```cpp
WM_HOTKEY
```

### Screenshot Capture

Options:

- Windows Graphics Capture: modern, good for Windows 10/11.
- DXGI Desktop Duplication: powerful, lower-level.
- GDI `BitBlt`: easiest MVP path, but less ideal for advanced cases.

MVP recommendation:

Start with GDI `BitBlt` for region capture. Move to Windows Graphics Capture later if needed.

### OCR

Recommended path:

1. Use Windows OCR locally for MVP.
2. Add Tesseract/PaddleOCR later if you need cross-platform or better support for difficult fonts.

Windows OCR namespace:

```text
Windows.Media.Ocr
```

In C++, use C++/WinRT to call Windows Runtime APIs.

OCR output should include:

- Full recognized text.
- Line text.
- Word text.
- Bounding boxes.

### Translation

For compatibility with the existing extension prototype, the first app version can call:

```text
https://translate.googleapis.com/translate_a/single
```

Request params:

```text
client=gtx
sl=auto
tl=<targetLanguage>
dt=t
q=<text>
```

Important:

This is not an official production API. For store/public release, use an official translation API or your own backend.

Production options:

- Google Cloud Translation API.
- Microsoft Translator.
- DeepL API.
- Your own backend with an open-source model.

### Overlay

Overlay requirements:

- Always on top.
- Transparent background.
- Click-through when idle.
- Can be dismissed with `Esc`.
- Does not steal focus unnecessarily.

Win32 styles to investigate:

```text
WS_EX_TOPMOST
WS_EX_LAYERED
WS_EX_TRANSPARENT
WS_EX_TOOLWINDOW
```

MVP overlay:

- Show one translated text panel near the selected region.
- Add later: per-line overlays using OCR bounding boxes.

## Main Workflow

```text
HotkeyManager receives Ctrl + Shift + T
-> AppController opens RegionSelector
-> user selects screen rectangle
-> CaptureManager captures bitmap
-> OcrEngine extracts text
-> if text is empty, show "No text found"
-> TranslationClient translates text
-> OverlayWindow shows translated text
```

## Language Detection

Avoid translating when source language already matches the target language.

Recommended:

- Use detected source language from the translation provider when available.
- Add lightweight local checks for Vietnamese if target is Vietnamese.

For Vietnamese:

- If text contains Vietnamese-specific characters, skip translation.
- Be careful with Vietnamese without diacritics because it can look like English.

## Privacy Design

Keep OCR local.

Only send extracted text to the translation service, not screenshots.

Settings:

- Add a privacy note in the app.
- Add an option to clear local history.
- Make history opt-in if it will be used for training data.

## Custom Model Roadmap

Do not start with model training.

Recommended roadmap:

1. Use an existing translation API.
2. Add glossary and term replacement.
3. Let users correct translations locally.
4. Export correction pairs as training data.
5. Fine-tune or deploy a custom model later.

Open-source model candidates:

- OPUS-MT / MarianMT for smaller language-pair models.
- NLLB for multilingual translation, but check license carefully before commercial use.

Deployment options:

- Hugging Face for quick prototype.
- Modal for serverless Python inference.
- RunPod Serverless for cheaper GPU inference.
- Dedicated GPU server for production control.

## MVP Milestones

### Milestone 1: App Shell

- Create Windows C++ project.
- Add tray icon.
- Add settings window.
- Store target language locally.

### Milestone 2: Hotkey

- Register `Ctrl + Shift + T`.
- Show a basic test overlay when hotkey is pressed.

### Milestone 3: Region Capture

- Add fullscreen transparent selection window.
- Let user drag a rectangle.
- Capture selected region to bitmap.

### Milestone 4: OCR

- Run OCR on captured bitmap.
- Show recognized text in a debug window or log.

### Milestone 5: Translation

- Send recognized text to translation API.
- Parse translated result.
- Handle API errors and empty text.

### Milestone 6: Overlay

- Render translated text near selected region.
- Add dismiss behavior.
- Keep overlay visually clean and readable.

### Milestone 7: Polish

- Add configurable hotkey.
- Add target language picker.
- Add skip-same-language behavior.
- Add local cache.
- Add installer.

## Suggested API Contract

If you create your own backend, keep the app/backend contract simple.

Request:

```json
{
  "text": "Hello world",
  "sourceLanguage": "auto",
  "targetLanguage": "vi"
}
```

Response:

```json
{
  "translatedText": "Xin chao the gioi",
  "detectedLanguage": "en"
}
```

## Local Cache

Cache translations by:

```text
targetLanguage + normalizedSourceText
```

Use an in-memory cache first. Add SQLite later if needed.

## Error Handling

Handle these cases:

- User cancels region selection.
- OCR finds no text.
- Translation API times out.
- Translation returns same language.
- Network is unavailable.
- Overlay appears outside screen bounds.

## First Build Recommendation

Use this sequence:

```text
1. C++ Windows app shell
2. global hotkey
3. region selector
4. screenshot capture
5. OCR local
6. translation API
7. overlay UI
```

That gives a working app before investing in custom model training.
