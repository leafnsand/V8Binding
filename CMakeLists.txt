cmake_minimum_required(VERSION 3.3)

project(V8Binding)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

include_directories(/usr/local /usr/local/include)

add_executable(V8Binding main.cpp
    include/CppArg.h
    include/CppBindClass.h
    include/CppBindModule.h
    include/CppFunction.h
    include/CppInvoke.h
    include/CppObject.h
    include/V8Type.h
)
find_library(libv8_base v8_base)
find_library(libv8_libbase v8_libbase)
find_library(libv8_nosnapshot v8_nosnapshot)
find_library(libv8_libplatform v8_libplatform)
set(libv8 ${libv8_base} ${libv8_libbase} ${libv8_nosnapshot} ${libv8_libplatform})
target_link_libraries(V8Binding ${libv8})