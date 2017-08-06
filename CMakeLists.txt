cmake_minimum_required(VERSION 3.6)
project(speakerman)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -DINSTALL_DIR='\"/usr/local\"'")
include_directories(include src/include)


set(SOURCE_FILES
        src/include/speakerman/jack/ErrorHandler.hpp
        src/include/speakerman/jack/JackClient.hpp
        src/include/speakerman/jack/JackProcessor.hpp
        src/include/speakerman/jack/Names.hpp
        src/include/speakerman/jack/Port.hpp
        src/include/speakerman/jack/PortDefinition.hpp
        src/include/speakerman/jack/SignalHandler.hpp
        src/include/speakerman/utils/Config.hpp
        src/include/speakerman/utils/Mutex.hpp
        src/include/speakerman/DynamicsProcessor.hpp
        src/include/speakerman/HttpMessage.hpp
        src/include/speakerman/ProcessingUnit.hpp
        src/include/speakerman/ServerSocket.hpp
        src/include/speakerman/SingleThreadFileCache.hpp
        src/include/speakerman/SocketStream.hpp
        src/include/speakerman/SpeakerManager.hpp
        src/include/speakerman/SpeakermanConfig.hpp
        src/include/speakerman/SpeakermanRuntimeData.hpp
        src/include/speakerman/SpeakermanWebServer.hpp
        src/include/speakerman/Stream.hpp
        src/include/tdap/AdvancedRmsDetector.hpp
        src/include/tdap/Array.hpp
        src/include/tdap/ArrayTraits.hpp
        src/include/tdap/CapacityPolicy.hpp
        src/include/tdap/Count.hpp
        src/include/tdap/Crossovers.hpp
        src/include/tdap/debug.hpp
        src/include/tdap/Delay.hpp
        src/include/tdap/Denormal.hpp
        src/include/tdap/Filters.hpp
        src/include/tdap/FixedCapArray.hpp
        src/include/tdap/FixedSizeArray.hpp
        src/include/tdap/Followers.hpp
        src/include/tdap/Frequency.hpp
        src/include/tdap/Guards.hpp
        src/include/tdap/IirBiquad.hpp
        src/include/tdap/IirButterworth.hpp
        src/include/tdap/IirCoefficients.hpp
        src/include/tdap/IndexPolicy.hpp
        src/include/tdap/Integration.hpp
        src/include/tdap/MemoryFence.hpp
        src/include/tdap/Noise.hpp
        src/include/tdap/Power2.hpp
        src/include/tdap/Rms.hpp
        src/include/tdap/Transport.hpp
        src/include/tdap/Value.hpp
        src/include/tdap/ValueRange.hpp
        src/include/tdap/VolumeMatrix.hpp
        src/include/tdap/Weighting.hpp
        src/ErrorHandler.cpp
        src/HttpMessage.cpp
        src/JackClient.cpp
        src/JackProcessor.cpp
        src/Names.cpp
        src/Port.cpp
        src/PortDefinition.cpp
        src/ProcessingUnit.cpp
        src/ServerSocket.cpp
        src/SignalHandler.cpp
        src/SingleThreadFileCache.cpp
        src/SocketStream.cpp
        src/speakerman.cpp
        src/SpeakermanConfig.cpp
        src/SpeakermanWebServer.cpp
        src/Stream.cpp)

add_executable(speakerman ${SOURCE_FILES})
set(LIBRARIES
        jack
        pthread
        )
link_libraries(jack pthread)
target_link_libraries(speakerman jack pthread)
add_executable(test_speakerman test/main.cpp)