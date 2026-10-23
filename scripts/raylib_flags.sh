# Sourced by scripts/build_raylib_*.sh: the raylib features the game uses, so
# the static archive (and the one-file game) carries nothing else.
#   - No clipboard images: on Linux that was raylib's only direct Xlib use.
#   - No audio file formats: the game streams the speaker's own samples.
#   - PNG only (modern mode's art); no image export, screenshots (F12),
#     compression API, automation events, fonts from files or 3D models.
#   - Function and data sections, so the game's link drops what is unused.
RAYLIB_TRIM_CFLAGS="-ffunction-sections -fdata-sections \
 -DSUPPORT_CLIPBOARD_IMAGE=0 \
 -DSUPPORT_FILEFORMAT_WAV=0 -DSUPPORT_FILEFORMAT_OGG=0 -DSUPPORT_FILEFORMAT_MP3=0 \
 -DSUPPORT_FILEFORMAT_QOA=0 -DSUPPORT_FILEFORMAT_FLAC=0 -DSUPPORT_FILEFORMAT_XM=0 -DSUPPORT_FILEFORMAT_MOD=0 \
 -DSUPPORT_FILEFORMAT_BMP=0 -DSUPPORT_FILEFORMAT_GIF=0 -DSUPPORT_FILEFORMAT_QOI=0 -DSUPPORT_FILEFORMAT_DDS=0 \
 -DSUPPORT_FILEFORMAT_TTF=0 -DSUPPORT_FILEFORMAT_FNT=0 \
 -DSUPPORT_IMAGE_EXPORT=0 -DSUPPORT_SCREEN_CAPTURE=0 -DSUPPORT_COMPRESSION_API=0 \
 -DSUPPORT_AUTOMATION_EVENTS=0 -DSUPPORT_MODULE_RMODELS=0"
