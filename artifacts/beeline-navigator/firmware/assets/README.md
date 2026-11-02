`background.png` is the original 320×240 wallpaper selected for the BeeLine display.
`../beeline_background.h` is the generated RGB565 version linked into flash by the
firmware. To replace it, regenerate the header with `../tools/generate-background.py`.
Regenerating the asset requires ImageMagick; compiling the sketch does not.
