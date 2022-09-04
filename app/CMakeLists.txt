add_executable(main_app "main.cpp")
target_link_libraries(main_app PRIVATE archive_parser project_config)
#target_link_libraries(main_app solver project_config)
