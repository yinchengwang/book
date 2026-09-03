add_library(rag_monitor STATIC
    monitoring.cpp
)
target_link_libraries(rag_monitor PUBLIC rag_core)
target_compile_features(rag_monitor PRIVATE cxx_std_17)
