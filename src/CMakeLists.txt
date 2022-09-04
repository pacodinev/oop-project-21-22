

find_package(Boost 1.63.0 REQUIRED COMPONENTS "filesystem")

add_library(archive_parser STATIC "archive_parser.cpp")
target_compile_features(archive_parser PUBLIC cxx_rvalue_references)
target_include_directories(archive_parser PUBLIC "../include" ${Boost_INCLUDE_DIR})
target_link_libraries(archive_parser PRIVATE LZW project_config ${Boost_FILESYSTEM_LIBRARY})

add_library(LZW STATIC "LZW.cpp")
target_compile_features(LZW PUBLIC cxx_rvalue_references cxx_std_11)
target_include_directories(LZW PUBLIC "../include" ${Boost_INCLUDE_DIR})
target_link_libraries(LZW PRIVATE compressor_base project_config)

#add_library(solver STATIC "solver.cpp")
#target_compile_features(solver PUBLIC cxx_rvalue_references cxx_final)
#target_include_directories(solver PUBLIC "../include")
#target_link_libraries(solver PRIVATE pstd_unordered_set project_config)
