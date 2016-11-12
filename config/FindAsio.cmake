# Finds asio.
#
# exports:
#
#   Asio_FOUND
#   Asio_INCLUDE_DIRS
#

include(FindPackageHandleStandardArgs)

# Include dir.
find_path(Asio_INCLUDE_DIRS
	NAMES
	asio.hpp
)

# Check if include dir was found.
find_package_handle_standard_args(Asio
	DEFAULT_MSG
	Asio_INCLUDE_DIRS
)

if (${ASIO_FOUND})
	set(Asio_FOUND true CACHE BOOL "" FORCE)
else()
	set(Asio_FOUND false CACHE BOOL "" FORCE)
endif()