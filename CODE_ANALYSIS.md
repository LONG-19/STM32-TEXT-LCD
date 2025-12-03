# Code Functionality Analysis

## Overview
This project targets the ALIENTEK STM32H743 board and drives an LCD to display GIF-based emoji states stored in SPI flash. The application synchronizes GIF assets from an SD card, initializes board peripherals, and continuously displays the image associated with the current command code.

## Asset Synchronization
- `copy_gif_to_flash` scans `0:/PICTURE` on the SD card, ensures `1:/PICTURE` exists on SPI flash, and copies only GIF files whose size differs or that do not yet exist in flash to minimize unnecessary writes. It streams data in 4 KB chunks and reports progress when debugging is enabled.【F:User/main.c†L112-L332】
- `flash_picture_first_init` can be called on first boot to invoke the synchronization routine, while `clear_flash_pictures` wipes the flash image directory if a reset is needed.【F:User/main.c†L335-L376】

## File Utilities
- `pic_get_tnum` opens a directory and counts image files (identified by `exfuns_file_type` returning a picture marker) to determine how many displayable assets are available.【F:User/main.c†L413-L449】

## Emotion Display Mapping
- A `GifEntry` table maps 1-byte emotion codes to specific GIF filenames stored in `1:/PICTURE`. `gif_get_name_by_code` resolves the filename for an incoming code, allowing `show_emotion_by_code` to load and render the GIF across the full LCD using `piclib_ai_load_picfile`.【F:User/main.c†L43-L90】【F:User/main.c†L451-L470】

## Application Flow
- The `main` routine initializes caches, clocks, UART, memory pools, storage mounts, LCD, and fonts before loading the GIF directory. It allocates buffers for file info and offset tracking, then builds an index of available images. After initializing the picture library and clearing the screen, it enters an infinite loop that reacts to serial commands (`AA 55 00 XX FB`) by updating the active emotion code and falls back to a default after 5 seconds of inactivity. Each iteration renders the GIF associated with the current code to the display.【F:User/main.c†L474-L642】

## Frame Rate Improvements
- The LTDC driver now allocates two SDRAM frame buffers and tracks distinct front/back indices. Rendering is done on the back buffer (`ltdc_set_draw_buffer`), and `ltdc_flip_buffers` atomically updates the LTDC base address before swapping indices so the freshly rendered frame becomes visible without tearing.【F:Drivers/BSP/LCD/ltdc.c†L14-L114】【F:Drivers/BSP/LCD/ltdc.c†L651-L712】
- GIF playback draws each frame to the back buffer, then flips to present it. This decouples decoding from the visible buffer, which avoids partial refreshes and allows DMA2D/LTDC to output the previous frame while the next one is prepared.【F:User/main.c†L454-L470】【F:Drivers/BSP/LCD/ltdc.h†L99-L118】
