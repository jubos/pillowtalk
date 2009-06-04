find_package( Boost 1.36.0 COMPONENTS test )
#include_directories(${CMAKE_CURRENT_BINARY_DIR}/../include/ ${Boost_INCLUDE_DIRS})
include_directories(${Boost_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/../src/)
#link_directories(${CMAKE_CURRENT_BINARY_DIR}/../lib)
link_libraries(${Boost_LIBARIES} pillowtalk)

set(HDRS ${CMAKE_CURRENT_SOURCE_DIR}/../src/pillowtalk.h)

add_executable(test_basic test_basic.cpp ${HDRS})
add_executable(test_json_generator test_json_generator.cpp ${HDRS})

enable_testing()

add_test(01_test_basic test_basic )
add_test(02_test_json_generator test_json_generator)
