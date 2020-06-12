cmake_minimum_required(VERSION 3.12)
project(apex_simulator)

set(CMAKE_CXX_STANDARD 14)

include_directories(.)

add_executable(apex_simulator
        cpu.cpp
        cpu.h
        file_parser.cpp
        helper.cpp
        helper.h
        main.cpp
        IQ.cpp
        IQ.h
        IQEntry.cpp
        IQEntry.h
        ROB.cpp
        ROB.h
        rob_entry.cpp
        rob_entry.h
        URF.h
        URF.cpp
        ForwardBus.cpp
        ForwardBus.h
        LSQ.cpp
        LSQ.h
        lsq_entry.cpp
        lsq_entry.h
        BTB.cpp
        BTB.h
        Makefile)
