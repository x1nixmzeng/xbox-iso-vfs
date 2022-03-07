cmake_minimum_required(VERSION 3.10)
project(xbox-iso-vfs)

set (CMAKE_CXX_STANDARD 17)

set(SOURCE_ROOT "${CMAKE_CURRENT_LIST_DIR}/src")
set(DOKAN_ROOT "${CMAKE_CURRENT_LIST_DIR}/third_party/Dokan")

set(SOURCE_FILES
	"${SOURCE_ROOT}/main.cc"
	"${SOURCE_ROOT}/xdvdfs.cc"
	"${SOURCE_ROOT}/vfs.cc"
	"${SOURCE_ROOT}/vfs_operations.cc"
)

set(HEADER_FILES
	"${SOURCE_ROOT}/xdvdfs.h"
	"${SOURCE_ROOT}/vfs.h"
	"${SOURCE_ROOT}/vfs_operations.h"
)

include_directories("${DOKAN_ROOT}/include")

add_executable(xbox-iso-vfs "${SOURCE_FILES}" "${HEADER_FILES}")

target_link_libraries(xbox-iso-vfs "${DOKAN_ROOT}/lib/dokan2.lib")

