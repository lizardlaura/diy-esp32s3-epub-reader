# Application component for the epub reader.
#
# The source list is board-conditional. Each board's .cpp pulls in the display
# driver headers for that board, so compiling all of them at once fails as soon
# as one driver component is absent. Set EPUB_BOARD to pick one.

if(NOT DEFINED EPUB_BOARD)
    set(EPUB_BOARD "LILYGO_T5_47_S3")
endif()

# Every source under src/, then remove the board files that don't apply.
# Filtered to real sources: the original glob (*.*) also swept up headers.
file(GLOB_RECURSE app_sources
     ${CMAKE_CURRENT_SOURCE_DIR}/*.c
     ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

if(EPUB_BOARD STREQUAL "LILYGO_T5_47_S3")
    # Uses LilyGo's driver. Drop everything that includes epdiy.h,
    # epd_highlevel.h, epd_board.h, or Renderer/EpdiyRenderer.h.
    list(FILTER app_sources EXCLUDE REGEX "/boards/PaperS3\\.cpp$")
    list(FILTER app_sources EXCLUDE REGEX "/boards/EpdiyPaperS3Board\\.c$")
    list(FILTER app_sources EXCLUDE REGEX "/boards/Epdiy\\.cpp$")
    list(FILTER app_sources EXCLUDE REGEX "/boards/Lilygo_t5_47\\.cpp$")
    list(FILTER app_sources EXCLUDE REGEX "/boards/M5Paper\\.cpp$")
    # Note: the files under boards/controls/ are all fine as-is.
    # PaperS3TouchControls.cpp needs only Renderer.h (and main.cpp includes
    # its header unconditionally, so it must stay), and L58TouchControls.cpp
    # is already behind #ifdef USE_L58_TOUCH.
    set(board_requires lilygo-epd47)
else()
    # epdiy-based boards: drop the LilyGo-only board.
    list(FILTER app_sources EXCLUDE REGEX "/boards/LilygoT5S3\\.cpp$")
    set(board_requires epdiy)
endif()

idf_component_register(SRCS ${app_sources} REQUIRES ${board_requires})

# 1. The ULP app name must be unique (if multiple components use ULP).
set(ulp_app_name ulp_main)
#
# 2. Specify all assembly source files.
#    Files should be placed into a separate directory (in this case, ulp/),
#    which should not be added to COMPONENT_SRCS.
set(ulp_s_sources "../ulp/main.S")
#
# 3. List all the component source files which include automatically
#    generated ULP export file, ${ulp_app_name}.h:
set(ulp_exp_dep_srcs "main.cpp")
#
# 4. Call function to build ULP binary and embed in project using the argument
#    values above.
ulp_embed_binary(${ulp_app_name} "${ulp_s_sources}" "${ulp_exp_dep_srcs}")