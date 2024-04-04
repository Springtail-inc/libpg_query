# our project is CXX, enable C
enable_language(C)

# find all .c files
file(GLOB SOURCES
     src/*.c
     src/postgres/*.c
)

# add additional .c files
list(APPEND SOURCES
     vendor/protobuf-c/protobuf-c.c
     vendor/xxhash/xxhash.c
     protobuf/pg_query.pb-c.c
)

add_library(pg_query STATIC ${SOURCES})
target_include_directories(pg_query
    PRIVATE vendor
    PUBLIC ./
    PUBLIC src
    PUBLIC src/include
    PUBLIC src/postgres/include
)
set_target_properties(pg_query PROPERTIES LINKER_LANGUAGE C)



