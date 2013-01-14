find_package( Boost COMPONENTS)
if (Boost_FOUND)
  include_directories(${Boost_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/../src/)
  link_libraries(${Boost_LIBARIES} pillowtalk)

  MESSAGE("-- Boost Include Dir:" ${Boost_INCLUDE_DIRS})

  set(HDRS ${CMAKE_CURRENT_SOURCE_DIR}/../src/pillowtalk.h)

  add_executable(test_basic test_basic.cpp ${HDRS})
  add_executable(test_json_generator test_json_generator.cpp ${HDRS})
  add_executable(test_iterator test_iterator.cpp ${HDRS})
  add_executable(test_parser test_parser.cpp ${HDRS})
  add_executable(test_changes_feed test_changes_feed.cpp ${HDRS})

  enable_testing()

  add_test(basic test_basic )
  add_test(json_generator test_json_generator ${CMAKE_CURRENT_SOURCE_DIR})
  add_test(iterator test_iterator)
  add_test(parser test_parser ${CMAKE_CURRENT_SOURCE_DIR})
else (Boost_FOUND)
  message("Install Boost to do testing")
endif (Boost_FOUND)
