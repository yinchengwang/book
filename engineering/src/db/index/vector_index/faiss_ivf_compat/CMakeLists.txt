# Faiss IVF 兼容层库

add_library(faiss_ivf_compat STATIC faiss_ivf_compat.c)

target_include_directories(faiss_ivf_compat PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${ENGINEERING_SOURCE_DIR}/include
)

target_link_libraries(faiss_ivf_compat PRIVATE project_includes Threads::Threads)