find_package( Boost )
if (Boost_FOUND)
  include_directories(${Boost_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/../src/)
  link_libraries(${Boost_LIBARIES} pillowtalk)

  set(HDRS ${CMAKE_CURRENT_SOURCE_DIR}/../src/pillowtalk.h)

  add_executable(test_basic test_basic.cpp ${HDRS})
  add_executable(test_json_generator test_json_generator.cpp ${HDRS})

  enable_testing()

  add_test(01_test_basic test_basic )
  add_test(02_test_json_generator test_json_generator)
else (Boost_FOUND)
  message("Install Boost to do testing")
endif (Boost_FOUND)
