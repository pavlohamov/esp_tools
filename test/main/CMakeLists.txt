idf_component_register(SRCS "main.cpp" INCLUDE_DIRS .)

list(APPEND cxx_compile_options "-std=gnu++17")

string(TIMESTAMP BUILD_DATE "%Y-%m-%d %H:%M:%S UTC" UTC)

FILE(GLOB SRC
	${CMAKE_CURRENT_SOURCE_DIR}/../*.c*

	${CMAKE_CURRENT_SOURCE_DIR}/../../impl/*.c*
)

target_include_directories(${COMPONENT_LIB} PRIVATE
	${CMAKE_BINARY_DIR}
	${CMAKE_CURRENT_BINARY_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/../../
)

target_sources(${COMPONENT_LIB} PRIVATE
	${SRC}
)
