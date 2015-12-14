cmake_minimum_required (VERSION 3.1)
project (ircxxd)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")

set (CMAKE_CXX_STANDARD 14)
include(FeatureSummary)

find_package(LibUV REQUIRED)

if(LibUV_FOUND)
	include_directories(${LIBUV_INCLUDE_DIRS})
endif()

find_package(Boost 1.58.0 COMPONENTS context)
set_package_properties(Boost PROPERTIES TYPE RECOMMENDED PURPOSE "Enables fast, thread-less Boost.Context yieldxx backend")
if(Boost_FOUND)
	add_executable (ircxxd_boost main.cpp uvxx/handle.cpp uvxx/stream.cpp uvxx/tcp.cpp yieldxx/coroutine_boost.cpp)
	include_directories(${Boost_INCLUDE_DIRS})
	target_link_libraries(ircxxd_boost ${LIBUV_LIBRARIES} ${Boost_LIBRARIES})
endif()

find_package(Threads)
set_package_properties(Threads PROPERTIES TYPE RECOMMENDED PURPOSE "Enables slow, thread-based yieldxx backend")
if(Threads_FOUND)
	add_executable (ircxxd_thread main.cpp uvxx/handle.cpp uvxx/stream.cpp uvxx/tcp.cpp yieldxx/coroutine_thread.cpp)
	target_link_libraries(ircxxd_thread ${LIBUV_LIBRARIES} Threads::Threads)
endif()

feature_summary(WHAT ALL)
