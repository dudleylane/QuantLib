add_executable(AuditHarness AuditHarness.cpp)
target_link_libraries(AuditHarness ql_library ${QL_THREAD_LIBRARIES})
