# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/cra/workplace_esp32/Komeiji_Satori/examples/motion_detection/managed_components/espressif__opencv/opencv"
  "/home/cra/workplace_esp32/Komeiji_Satori/examples/motion_detection/build/esp-idf/espressif__opencv/opencv-build"
  "/home/cra/workplace_esp32/Komeiji_Satori/examples/motion_detection/build/esp-idf/espressif__opencv/opencv_proj-prefix"
  "/home/cra/workplace_esp32/Komeiji_Satori/examples/motion_detection/build/esp-idf/espressif__opencv/opencv_proj-prefix/tmp"
  "/home/cra/workplace_esp32/Komeiji_Satori/examples/motion_detection/build/esp-idf/espressif__opencv/opencv_proj-prefix/src/opencv_proj-stamp"
  "/home/cra/workplace_esp32/Komeiji_Satori/examples/motion_detection/build/esp-idf/espressif__opencv/opencv_proj-prefix/src"
  "/home/cra/workplace_esp32/Komeiji_Satori/examples/motion_detection/build/esp-idf/espressif__opencv/opencv_proj-prefix/src/opencv_proj-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/cra/workplace_esp32/Komeiji_Satori/examples/motion_detection/build/esp-idf/espressif__opencv/opencv_proj-prefix/src/opencv_proj-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/cra/workplace_esp32/Komeiji_Satori/examples/motion_detection/build/esp-idf/espressif__opencv/opencv_proj-prefix/src/opencv_proj-stamp${cfgdir}") # cfgdir has leading slash
endif()
