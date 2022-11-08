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

target_precompile_headers(LLVMDemangle PRIVATE <cstring> <cstddef> <cstdint> <cmath> <algorithm> <string> <utility> <type_traits> <vector> <memory> <iterator> <variant> <limits> <optional> <chrono>)
