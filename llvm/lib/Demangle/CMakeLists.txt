add_llvm_component_library(LLVMDemangle
  Demangle.cpp
  ItaniumDemangle.cpp
  MicrosoftDemangle.cpp
  MicrosoftDemangleNodes.cpp
  RustDemangle.cpp
  DLangDemangle.cpp

  ADDITIONAL_HEADER_DIRS
  "${LLVM_MAIN_INCLUDE_DIR}/llvm/Demangle"

)

if(NOT MSVC AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    list(APPEND CMAKE_CXX_COMPILE_OPTIONS_CREATE_PCH -Xclang -fno-pch-timestamp)
endif()
target_precompile_headers(LLVMDemangle PRIVATE $<$<COMPILE_LANGUAGE:CXX>:pch.h>)
