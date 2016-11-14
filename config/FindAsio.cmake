# Finds asio.
#
# variables:
#
#	ASIO_ROOT
#
# exports:
#
#   Asio_FOUND
#   Asio_INCLUDE_DIRS
#   Asio_VERSION
#

include(FindPackageHandleStandardArgs)

# Include dir.
find_path(Asio_INCLUDE_DIRS
	NAMES
	asio.hpp
	HINTS
	${ASIO_ROOT}
)

# Check version
set(Asio_version_h "${Asio_INCLUDE_DIRS}/asio/version.hpp")
set(Asio_version_expr "^[ \\t]*#define[ \\t]ASIO_VERSION[ \\t]+([0-9]+).+$")
file(STRINGS "${Asio_version_h}" Asio_version_line REGEX "${Asio_version_expr}")
string(REGEX MATCH "${Asio_version_expr}" Asio_version "${Asio_version_line}")
math(EXPR Asio_version_patch "${CMAKE_MATCH_1} % 100")
math(EXPR Asio_version_minor "${CMAKE_MATCH_1} / 100 % 1000")
math(EXPR Asio_version_major "${CMAKE_MATCH_1} / 100000")
set(Asio_VERSION "${Asio_version_major}.${Asio_version_minor}.${Asio_version_patch}")

# Check if include dir was found.
find_package_handle_standard_args(Asio
	FOUND_VAR
	Asio_FOUND
	REQUIRED_VARS
	Asio_INCLUDE_DIRS
	VERSION_VAR
	Asio_VERSION
)

set(Asio_VERSION ${Asio_VERSION} CACHE STRING "" FORCE)
set(Asio_FOUND ${Asio_FOUND} CACHE BOOL "" FORCE)
