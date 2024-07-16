INCLUDE(FindPkgConfig)
PKG_CHECK_MODULES(PC_CRAN cran)

FIND_PATH(
    GR_CRAN_INCLUDE_DIRS
    NAMES gnuradio/cran/api.h
    HINTS $ENV{CRAN_DIR}/include
        ${PC_CRAN_INCLUDEDIR}
    PATHS ${CMAKE_INSTALL_PREFIX}/include
          /usr/local/include
          /usr/include
)

FIND_LIBRARY(
    GR_CRAN_LIBRARIES
    NAMES gnuradio-cran
    HINTS $ENV{CRAN_DIR}/lib
        ${PC_CRAN_LIBDIR}
    PATHS ${CMAKE_INSTALL_PREFIX}/lib
          ${CMAKE_INSTALL_PREFIX}/lib64
          /usr/local/lib
          /usr/local/lib64
          /usr/lib
          /usr/lib64
          )

include("${CMAKE_CURRENT_LIST_DIR}/gnuradio-cranTarget.cmake")

INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(CRAN DEFAULT_MSG CRAN_LIBRARIES CRAN_INCLUDE_DIRS)
MARK_AS_ADVANCED(GR_CRAN_LIBRARIES GR_CRAN_INCLUDE_DIRS)
