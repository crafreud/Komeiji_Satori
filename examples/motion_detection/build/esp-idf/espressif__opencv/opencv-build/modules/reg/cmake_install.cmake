# Install script for directory: /home/cra/workplace_esp32/Komeiji_Satori/examples/motion_detection/managed_components/espressif__opencv/opencv_contrib/modules/reg

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
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY OPTIONAL FILES "/home/cra/workplace_esp32/Komeiji_Satori/examples/motion_detection/build/esp-idf/espressif__opencv/opencv-build/lib/libopencv_reg.a")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "dev" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/opencv4/opencv2/reg" TYPE FILE OPTIONAL FILES "/home/cra/workplace_esp32/Komeiji_Satori/examples/motion_detection/managed_components/espressif__opencv/opencv_contrib/modules/reg/include/opencv2/reg/map.hpp")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "dev" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/opencv4/opencv2/reg" TYPE FILE OPTIONAL FILES "/home/cra/workplace_esp32/Komeiji_Satori/examples/motion_detection/managed_components/espressif__opencv/opencv_contrib/modules/reg/include/opencv2/reg/mapaffine.hpp")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "dev" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/opencv4/opencv2/reg" TYPE FILE OPTIONAL FILES "/home/cra/workplace_esp32/Komeiji_Satori/examples/motion_detection/managed_components/espressif__opencv/opencv_contrib/modules/reg/include/opencv2/reg/mapper.hpp")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "dev" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/opencv4/opencv2/reg" TYPE FILE OPTIONAL FILES "/home/cra/workplace_esp32/Komeiji_Satori/examples/motion_detection/managed_components/espressif__opencv/opencv_contrib/modules/reg/include/opencv2/reg/mappergradaffine.hpp")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "dev" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/opencv4/opencv2/reg" TYPE FILE OPTIONAL FILES "/home/cra/workplace_esp32/Komeiji_Satori/examples/motion_detection/managed_components/espressif__opencv/opencv_contrib/modules/reg/include/opencv2/reg/mappergradeuclid.hpp")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "dev" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/opencv4/opencv2/reg" TYPE FILE OPTIONAL FILES "/home/cra/workplace_esp32/Komeiji_Satori/examples/motion_detection/managed_components/espressif__opencv/opencv_contrib/modules/reg/include/opencv2/reg/mappergradproj.hpp")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "dev" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/opencv4/opencv2/reg" TYPE FILE OPTIONAL FILES "/home/cra/workplace_esp32/Komeiji_Satori/examples/motion_detection/managed_components/espressif__opencv/opencv_contrib/modules/reg/include/opencv2/reg/mappergradshift.hpp")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "dev" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/opencv4/opencv2/reg" TYPE FILE OPTIONAL FILES "/home/cra/workplace_esp32/Komeiji_Satori/examples/motion_detection/managed_components/espressif__opencv/opencv_contrib/modules/reg/include/opencv2/reg/mappergradsimilar.hpp")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "dev" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/opencv4/opencv2/reg" TYPE FILE OPTIONAL FILES "/home/cra/workplace_esp32/Komeiji_Satori/examples/motion_detection/managed_components/espressif__opencv/opencv_contrib/modules/reg/include/opencv2/reg/mapperpyramid.hpp")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "dev" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/opencv4/opencv2/reg" TYPE FILE OPTIONAL FILES "/home/cra/workplace_esp32/Komeiji_Satori/examples/motion_detection/managed_components/espressif__opencv/opencv_contrib/modules/reg/include/opencv2/reg/mapprojec.hpp")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "dev" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/opencv4/opencv2/reg" TYPE FILE OPTIONAL FILES "/home/cra/workplace_esp32/Komeiji_Satori/examples/motion_detection/managed_components/espressif__opencv/opencv_contrib/modules/reg/include/opencv2/reg/mapshift.hpp")
endif()

