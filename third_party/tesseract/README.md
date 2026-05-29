# Bundled Tesseract Runtime

Place the Tesseract CLI runtime in this folder:

```text
third_party/tesseract/tesseract.exe
third_party/tesseract/tessdata/*.traineddata
```

The app calls:

```text
tesseract.exe <capture.bmp> stdout -l <language> --tessdata-dir <tessdata> --psm 6
```

Suggested language files:

- `eng.traineddata`
- `vie.traineddata`
- `jpn.traineddata`
- `kor.traineddata`
- `chi_sim.traineddata`
- `chi_tra.traineddata`
- `fra.traineddata`
- `deu.traineddata`
- `spa.traineddata`
- `rus.traineddata`
- `tha.traineddata`
