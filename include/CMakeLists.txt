#add_library(tree_serializer INTERFACE)
#target_compile_features(tree_serializer INTERFACE cxx_rvalue_references)
#target_include_directories(tree_serializer INTERFACE "../include")
#target_link_libraries(tree_serializer INTERFACE project_config)

add_library(compressor_base INTERFACE)
target_compile_features(compressor_base INTERFACE cxx_std_11)
target_include_directories(compressor_base INTERFACE ".")
target_link_libraries(compressor_base INTERFACE project_config)

