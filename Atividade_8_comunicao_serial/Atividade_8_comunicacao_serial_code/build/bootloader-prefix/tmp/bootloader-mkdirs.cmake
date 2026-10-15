# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION 3.5)

file(MAKE_DIRECTORY
  "/home/edriel/.espressif/v6.1-beta1/esp-idf/components/bootloader/subproject"
  "/home/edriel/workspace_esp/Atividade_8_comunicao_serial_code/build/bootloader"
  "/home/edriel/workspace_esp/Atividade_8_comunicao_serial_code/build/bootloader-prefix"
  "/home/edriel/workspace_esp/Atividade_8_comunicao_serial_code/build/bootloader-prefix/tmp"
  "/home/edriel/workspace_esp/Atividade_8_comunicao_serial_code/build/bootloader-prefix/src/bootloader-stamp"
  "/home/edriel/workspace_esp/Atividade_8_comunicao_serial_code/build/bootloader-prefix/src"
  "/home/edriel/workspace_esp/Atividade_8_comunicao_serial_code/build/bootloader-prefix/src/bootloader-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "/home/edriel/workspace_esp/Atividade_8_comunicao_serial_code/build/bootloader-prefix/src/bootloader-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "/home/edriel/workspace_esp/Atividade_8_comunicao_serial_code/build/bootloader-prefix/src/bootloader-stamp${cfgdir}") # cfgdir has leading slash
endif()
