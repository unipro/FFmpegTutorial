cmake_minimum_required(VERSION 3.14)

project(FFmpegTutorial)

# ffmpeg
find_path(AVFORMAT_INCLUDE_DIR libavformat/avformat.h)
find_library(AVFORMAT_LIBRARY avformat)

find_path(AVCODEC_INCLUDE_DIR libavcodec/avcodec.h)
find_library(AVCODEC_LIBRARY avcodec)

find_path(AVUTIL_INCLUDE_DIR libavutil/avutil.h)
find_library(AVUTIL_LIBRARY avutil)

find_path(AVFILTER_INCLUDE_DIR libavfilter/avfilter.h)
find_library(AVFILTER_LIBRARY avfilter)

# sample01_scanning
add_executable(sample01_scanning sample01_scanning.c)
target_include_directories(sample01_scanning PRIVATE ${AVFORMAT_INCLUDE_DIR} ${AVCODEC_INCLUDE_DIR} ${AVUTIL_INCLUDE_DIR})
target_link_libraries(sample01_scanning PRIVATE ${AVFORMAT_LIBRARY} ${AVCODEC_LIBRARY} ${AVUTIL_LIBRARY})

# sample02_demuxing
add_executable(sample02_demuxing sample02_demuxing.c)
target_include_directories(sample02_demuxing PRIVATE ${AVFORMAT_INCLUDE_DIR} ${AVCODEC_INCLUDE_DIR} ${AVUTIL_INCLUDE_DIR})
target_link_libraries(sample02_demuxing PRIVATE ${AVFORMAT_LIBRARY} ${AVCODEC_LIBRARY} ${AVUTIL_LIBRARY})

# sample03_remuxing
add_executable(sample03_remuxing sample03_remuxing.c)
target_include_directories(sample03_remuxing PRIVATE ${AVFORMAT_INCLUDE_DIR} ${AVCODEC_INCLUDE_DIR} ${AVUTIL_INCLUDE_DIR})
target_link_libraries(sample03_remuxing PRIVATE ${AVFORMAT_LIBRARY} ${AVCODEC_LIBRARY} ${AVUTIL_LIBRARY})

# sample04_decoding
add_executable(sample04_decoding sample04_decoding.c)
target_include_directories(sample04_decoding PRIVATE ${AVFORMAT_INCLUDE_DIR} ${AVCODEC_INCLUDE_DIR} ${AVUTIL_INCLUDE_DIR})
target_link_libraries(sample04_decoding PRIVATE ${AVFORMAT_LIBRARY} ${AVCODEC_LIBRARY} ${AVUTIL_LIBRARY})

# sample05_filtering
add_executable(sample05_filtering sample05_filtering.c)
target_include_directories(sample05_filtering PRIVATE ${AVFORMAT_INCLUDE_DIR} ${AVCODEC_INCLUDE_DIR} ${AVUTIL_INCLUDE_DIR} ${AVFILTER_INCLUDE_DIR})
target_link_libraries(sample05_filtering PRIVATE ${AVFORMAT_LIBRARY} ${AVCODEC_LIBRARY} ${AVUTIL_LIBRARY} ${AVFILTER_LIBRARY})

# sample06_encoding
add_executable(sample06_encoding sample06_encoding.c)
target_include_directories(sample06_encoding PRIVATE ${AVFORMAT_INCLUDE_DIR} ${AVCODEC_INCLUDE_DIR} ${AVUTIL_INCLUDE_DIR} ${AVFILTER_INCLUDE_DIR})
target_link_libraries(sample06_encoding PRIVATE ${AVFORMAT_LIBRARY} ${AVCODEC_LIBRARY} ${AVUTIL_LIBRARY} ${AVFILTER_LIBRARY})
