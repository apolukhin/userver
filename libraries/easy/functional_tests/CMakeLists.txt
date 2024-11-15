project(userver-easy-tests CXX)

add_custom_target(${PROJECT_NAME})

add_subdirectory(sample)
add_dependencies(${PROJECT_NAME} ${PROJECT_NAME}-sample)
