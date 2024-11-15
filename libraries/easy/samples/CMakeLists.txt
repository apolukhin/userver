project(userver-easy-samples CXX)

add_custom_target(${PROJECT_NAME})

add_subdirectory(0_hello_world)
add_dependencies(${PROJECT_NAME} ${PROJECT_NAME}-hello-world)
