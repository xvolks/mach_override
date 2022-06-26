set(FOLDER ${CMAKE_CURRENT_LIST_DIR})

if(${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.11.0")
    file(GLOB_RECURSE SOURCE_FILES ${FOLDER}/*.cpp ${FOLDER}/*.c ${FOLDER}/*.mm ${FOLDER}/*.h ${FOLDER}/*.hpp)
else()
    file(GLOB SOURCE_FILES ${FOLDER}/*.cpp ${FOLDER}/*.mm ${FOLDER}/*.h ${FOLDER}/*.hpp ${FOLDER}/*/*.hpp ${FOLDER}/*/*.h ${FOLDER}/*/*.cpp)
endif()

message("The current folder is ${FOLDER} :")
message("       FILES : ${SOURCE_FILES}")
list(APPEND SOURCE ${SOURCE_FILES})