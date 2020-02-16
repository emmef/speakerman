cmake_minimum_required(VERSION 3.6)
project(speakerman)

set(CMAKE_CXX_STANDARD 17)

find_path(LOCAL_TDAP_PATH, README-TAP.md HINTS ../ ../../ NO_DEFAULT_PATH)

include_directories(include src/include ${LOCAL_TDAP_PATH}/src /usr/include)
set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)
find_package(Boost)

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
        src/include/speakerman/ServerSocket.hpp
        src/include/speakerman/SingleThreadFileCache.hpp
        src/include/speakerman/SocketStream.hpp
        src/include/speakerman/SpeakerManager.hpp
        src/include/speakerman/SpeakermanConfig.hpp
        src/include/speakerman/SpeakermanRuntimeData.hpp
        src/include/speakerman/SpeakermanWebServer.hpp
        src/include/speakerman/Stream.hpp
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
        src/include/tdap/PeakDetection.hpp
        src/include/tdap/PerceptiveRms.hpp
        src/include/tdap/Samples.hpp
        src/include/tdap/Transport.hpp
        src/include/tdap/TrueFloatingPointWindowAverage.hpp
        src/include/tdap/Value.hpp
        src/include/tdap/ValueRange.hpp
        src/include/tdap/VolumeMatrix.hpp
        src/include/tdap/Weighting.hpp
        src/Allocation.cpp
        src/ErrorHandler.cpp
        src/HttpMessage.cpp
        src/JackClient.cpp
        src/JackProcessor.cpp
        src/Names.cpp
        src/Port.cpp
        src/PortDefinition.cpp
        src/ServerSocket.cpp
        src/SignalHandler.cpp
        src/SingleThreadFileCache.cpp
        src/SocketStream.cpp
        src/SpeakermanConfig.cpp
        src/SpeakermanWebServer.cpp
        src/Stream.cpp src/include/tdap/Limiter.hpp src/include/tdap/AlignedFrame.hpp)

find_path(LOCAL_TDAP_PATH, README-TAP.md HINTS ../ ../../ NO_DEFAULT_PATH)


add_executable(speakerman ${SOURCE_FILES} src/speakerman.cpp)
set(LIBRARIES
        jack
        pthread
        )
link_libraries(jack pthread)
target_link_libraries(speakerman jack pthread)
add_executable(test_speakerman ${SOURCE_FILES} test/main.cpp test/TestIirCoefficients.hpp test/TestIirCoefficients.cpp test/TestAlignedFrame.cpp test/TestAlignedFrame.hpp)
install(TARGETS speakerman RUNTIME DESTINATION bin)
install(FILES web/index.html web/speakerman.js web/speakerman.css DESTINATION share/speakerman/web)
install(PROGRAMS src/speakerman-watchdog.sh DESTINATION share/speakerman/script)
install(FILES service/speakerman.service DESTINATION /lib/systemd/system)

add_definitions(-DSPEAKERMAN_INSTALL_PREFIX=${CMAKE_INSTALL_PREFIX})
