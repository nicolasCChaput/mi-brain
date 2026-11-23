#-----------------------------------------------------------------------------
# trx-cpp (https://github.com/tee-ar-ex/trx-cpp) reads and writes .trx files
# (see FiberBundle/TrxRead.cpp and TrxWrite.cpp). It is built here from a local source checkout.
#-----------------------------------------------------------------------------
set(MIBRAIN_TRX_CPP_DIR "" CACHE PATH "Path to a trx-cpp source checkout")

if(NOT TARGET trx-cpp::trx)
  if(NOT EXISTS "${MIBRAIN_TRX_CPP_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
      "trx-cpp not found. Set MIBRAIN_TRX_CPP_DIR to a trx-cpp source checkout "
      "(currently: '${MIBRAIN_TRX_CPP_DIR}').")
  endif()

  # trx-cpp must use the Eigen shipped with ITK, otherwise the two Eigen versions conflict at build time.
  find_package(Eigen3 REQUIRED CONFIG NO_DEFAULT_PATH PATHS "${ITK_DIR}/Modules")
  set(TRX_EIGEN3_TARGET Eigen3::Eigen)

  set(TRX_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(TRX_ENABLE_NIFTI OFF CACHE BOOL "" FORCE)
  set(TRX_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)

  # Build trx statically into FiberBundle so there is no extra .so to deploy.
  set(_mibrain_saved_BUILD_SHARED_LIBS ${BUILD_SHARED_LIBS})
  set(BUILD_SHARED_LIBS OFF)
  add_subdirectory("${MIBRAIN_TRX_CPP_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/trx-cpp-build" EXCLUDE_FROM_ALL)
  set(BUILD_SHARED_LIBS ${_mibrain_saved_BUILD_SHARED_LIBS})
  set_target_properties(trx PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()

MITK_CREATE_MODULE( FiberBundle
  PACKAGE_DEPENDS PUBLIC Qt6|Core OpenMP|OpenMP_CXX VTK|CommonComputationalGeometry
  DEPENDS MitkCore
  WARNINGS_NO_ERRORS
)

target_link_libraries(${MODULE_TARGET} PRIVATE trx-cpp::trx)

add_definitions(-DBOOST_TIMER_ENABLE_DEPRECATED)
