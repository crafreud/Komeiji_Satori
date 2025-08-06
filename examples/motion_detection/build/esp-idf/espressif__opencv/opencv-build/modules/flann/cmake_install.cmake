# Install script for directory: /home/cra/workplace_esp32/Komeiji_Satori/examples/motion_detection/managed_components/espressif__opencv/opencv/modules/flann

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/home/cra/workplace_esp32/Komeiji_Satori/examples/motion_detection/build/esp-idf/espressif__opencv/opencv-build/install")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Release")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "TRUE")
endif()

# Set default install directory permissions.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "/home/cra/.espressif/tools/xtensa-esp-elf/esp-14.2.0_20241119/xtensa-esp-elf/bin/xtensa-esp32s3-elf-objdump")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "dev" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY OPTIONAL FILES "/home/cra/workplace_esp32/Komeiji_Satori/examples/motion_detection/build/esp-idf/espressif__opencv/opencv-build/lib/libopencv_flann.a")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "dev" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/opencv4/opencv2" TYPE FILE OPTIONAL FILES "/home/cra/workplace_esp32/Komeiji_Satori/examples/motion_detection/managed_components/espressif__opencv/opencv/modules/flann/include/opencv2/flann.hpp")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "dev" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/opencv4/opencv2/flann" TYPE FILE OPTIONAL FILES "/home/cra/workplace_esp32/Komeiji_Satori/examples/motion_detection/managed_components/espressif__opencv/opencv/modules/flann/include/opencv2/flann/all_indices.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "dev" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/opencv4/opencv2/flann" TYPE FILE OPTIONAL FILES "/home/cra/workplace_esp32/Komeiji_Satori/examples/motion_detection/managed_components/espressif__opencv/opencv/modules/flann/include/opencv2/flann/allocator.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "dev" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/opencv4/opencv2/flann" TYPE FILE OPTIONAL FILES "/home/cra/workplace_esp32/Komeiji_Satori/examples/motion_detection/managed_components/espressif__opencv/opencv/modules/flann/include/opencv2/flann/any.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "dev" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/opencv4/opencv2/flann" TYPE FILE OPTIONAL FILES "/home/cra/workplace_esp32/Komeiji_Satori/examples/motion_detection/managed_components/espressif__opencv/opencv/modules/flann/include/opencv2/flann/autotuned_index.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "dev" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/opencv4/opencv2/flann" TYPE FILE OPTIONAL FILES "/home/cra/workplace_esp32/Komeiji_Satori/examples/motion_detection/managed_components/espressif__opencv/opencv/modules/flann/include/opencv2/flann/composite_index.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "dev" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/opencv4/opencv2/flann" TYPE FILE OPTIONAL FILES "/home/cra/workplace_esp32/Komeiji_Satori/examples/motion_detection/managed_components/espressif__opencv/opencv/modules/flann/include/opencv2/flann/config.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "dev" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/opencv4/opencv2/flann" TYPE FILE OPTIONAL FILES "/home/cra/workplace_esp32/Komeiji_Satori/examples/motion_detection/managed_components/espressif__opencv/opencv/modules/flann/include/opencv2/flann/defines.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "dev" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/opencv4/opencv2/flann" TYPE FILE OPTIONAL FILES "/home/cra/workplace_esp32/Komeiji_Satori/examples/motion_detection/managed_components/espressif__opencv/opencv/modules/flann/include/opencv2/flann/dist.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "dev" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/opencv4/opencv2/flann" TYPE FILE OPTIONAL FILES "/home/cra/workplace_esp32/Komeiji_Satori/examples/motion_detection/managed_components/espressif__opencv/opencv/modules/flann/include/opencv2/flann/dummy.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "dev" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/opencv4/opencv2/flann" TYPE FILE OPTIONAL FILES "/home/cra/workplace_esp32/Komeiji_Satori/examples/motion_detection/managed_components/espressif__opencv/opencv/modules/flann/include/opencv2/flann/dynamic_bitset.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "dev" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/opencv4/opencv2/flann" TYPE FILE OPTIONAL FILES "/home/cra/workplace_esp32/Komeiji_Satori/examples/motion_detection/managed_components/espressif__opencv/opencv/modules/flann/include/opencv2/flann/flann.hpp")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "dev" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/opencv4/opencv2/flann" TYPE FILE OPTIONAL FILES "/home/cra/workplace_esp32/Komeiji_Satori/examples/motion_detection/managed_components/espressif__opencv/opencv/modules/flann/include/opencv2/flann/flann_base.hpp")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "dev" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/opencv4/opencv2/flann" TYPE FILE OPTIONAL FILES "/home/cra/workplace_esp32/Komeiji_Satori/examples/motion_detection/managed_components/espressif__opencv/opencv/modules/flann/include/opencv2/flann/general.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "dev" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/opencv4/opencv2/flann" TYPE FILE OPTIONAL FILES "/home/cra/workplace_esp32/Komeiji_Satori/examples/motion_detection/managed_components/espressif__opencv/opencv/modules/flann/include/opencv2/flann/ground_truth.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "dev" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/opencv4/opencv2/flann" TYPE FILE OPTIONAL FILES "/home/cra/workplace_esp32/Komeiji_Satori/examples/motion_detection/managed_components/espressif__opencv/opencv/modules/flann/include/opencv2/flann/hdf5.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "dev" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/opencv4/opencv2/flann" TYPE FILE OPTIONAL FILES "/home/cra/workplace_esp32/Komeiji_Satori/examples/motion_detection/managed_components/espressif__opencv/opencv/modules/flann/include/opencv2/flann/heap.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "dev" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/opencv4/opencv2/flann" TYPE FILE OPTIONAL FILES "/home/cra/workplace_esp32/Komeiji_Satori/examples/motion_detection/managed_components/espressif__opencv/opencv/modules/flann/include/opencv2/flann/hierarchical_clustering_index.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "dev" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/opencv4/opencv2/flann" TYPE FILE OPTIONAL FILES "/home/cra/workplace_esp32/Komeiji_Satori/examples/motion_detection/managed_components/espressif__opencv/opencv/modules/flann/include/opencv2/flann/index_testing.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "dev" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/opencv4/opencv2/flann" TYPE FILE OPTIONAL FILES "/home/cra/workplace_esp32/Komeiji_Satori/examples/motion_detection/managed_components/espressif__opencv/opencv/modules/flann/include/opencv2/flann/kdtree_index.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "dev" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/opencv4/opencv2/flann" TYPE FILE OPTIONAL FILES "/home/cra/workplace_esp32/Komeiji_Satori/examples/motion_detection/managed_components/espressif__opencv/opencv/modules/flann/include/opencv2/flann/kdtree_single_index.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "dev" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/opencv4/opencv2/flann" TYPE FILE OPTIONAL FILES "/home/cra/workplace_esp32/Komeiji_Satori/examples/motion_detection/managed_components/espressif__opencv/opencv/modules/flann/include/opencv2/flann/kmeans_index.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "dev" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/opencv4/opencv2/flann" TYPE FILE OPTIONAL FILES "/home/cra/workplace_esp32/Komeiji_Satori/examples/motion_detection/managed_components/espressif__opencv/opencv/modules/flann/include/opencv2/flann/linear_index.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "dev" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/opencv4/opencv2/flann" TYPE FILE OPTIONAL FILES "/home/cra/workplace_esp32/Komeiji_Satori/examples/motion_detection/managed_components/espressif__opencv/opencv/modules/flann/include/opencv2/flann/logger.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "dev" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/opencv4/opencv2/flann" TYPE FILE OPTIONAL FILES "/home/cra/workplace_esp32/Komeiji_Satori/examples/motion_detection/managed_components/espressif__opencv/opencv/modules/flann/include/opencv2/flann/lsh_index.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "dev" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/opencv4/opencv2/flann" TYPE FILE OPTIONAL FILES "/home/cra/workplace_esp32/Komeiji_Satori/examples/motion_detection/managed_components/espressif__opencv/opencv/modules/flann/include/opencv2/flann/lsh_table.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "dev" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/opencv4/opencv2/flann" TYPE FILE OPTIONAL FILES "/home/cra/workplace_esp32/Komeiji_Satori/examples/motion_detection/managed_components/espressif__opencv/opencv/modules/flann/include/opencv2/flann/matrix.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "dev" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/opencv4/opencv2/flann" TYPE FILE OPTIONAL FILES "/home/cra/workplace_esp32/Komeiji_Satori/examples/motion_detection/managed_components/espressif__opencv/opencv/modules/flann/include/opencv2/flann/miniflann.hpp")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "dev" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/opencv4/opencv2/flann" TYPE FILE OPTIONAL FILES "/home/cra/workplace_esp32/Komeiji_Satori/examples/motion_detection/managed_components/espressif__opencv/opencv/modules/flann/include/opencv2/flann/nn_index.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "dev" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/opencv4/opencv2/flann" TYPE FILE OPTIONAL FILES "/home/cra/workplace_esp32/Komeiji_Satori/examples/motion_detection/managed_components/espressif__opencv/opencv/modules/flann/include/opencv2/flann/object_factory.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "dev" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/opencv4/opencv2/flann" TYPE FILE OPTIONAL FILES "/home/cra/workplace_esp32/Komeiji_Satori/examples/motion_detection/managed_components/espressif__opencv/opencv/modules/flann/include/opencv2/flann/params.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "dev" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/opencv4/opencv2/flann" TYPE FILE OPTIONAL FILES "/home/cra/workplace_esp32/Komeiji_Satori/examples/motion_detection/managed_components/espressif__opencv/opencv/modules/flann/include/opencv2/flann/random.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "dev" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/opencv4/opencv2/flann" TYPE FILE OPTIONAL FILES "/home/cra/workplace_esp32/Komeiji_Satori/examples/motion_detection/managed_components/espressif__opencv/opencv/modules/flann/include/opencv2/flann/result_set.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "dev" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/opencv4/opencv2/flann" TYPE FILE OPTIONAL FILES "/home/cra/workplace_esp32/Komeiji_Satori/examples/motion_detection/managed_components/espressif__opencv/opencv/modules/flann/include/opencv2/flann/sampling.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "dev" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/opencv4/opencv2/flann" TYPE FILE OPTIONAL FILES "/home/cra/workplace_esp32/Komeiji_Satori/examples/motion_detection/managed_components/espressif__opencv/opencv/modules/flann/include/opencv2/flann/saving.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "dev" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/opencv4/opencv2/flann" TYPE FILE OPTIONAL FILES "/home/cra/workplace_esp32/Komeiji_Satori/examples/motion_detection/managed_components/espressif__opencv/opencv/modules/flann/include/opencv2/flann/simplex_downhill.h")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "dev" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/opencv4/opencv2/flann" TYPE FILE OPTIONAL FILES "/home/cra/workplace_esp32/Komeiji_Satori/examples/motion_detection/managed_components/espressif__opencv/opencv/modules/flann/include/opencv2/flann/timer.h")
endif()

