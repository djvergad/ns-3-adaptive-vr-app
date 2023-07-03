set(source_files
    model/burst-generator.cc
    model/burst-sink.cc
    model/bursty-application.cc
    model/my-random-variable-stream.cc
    model/seq-ts-size-frag-header.cc
    model/simple-burst-generator.cc
    model/trace-file-burst-generator.cc
    model/vr-adaptive-burst-sink.cc
    model/vr-adaptive-bursty-application.cc
    model/vr-adaptive-header.cc
    model/vr-burst-generator.cc
    model/burst-sink-tcp.cc
    model/bursty-application-tcp.cc
    model/vr-adaptive-burst-sink-tcp.cc
    model/vr-adaptive-bursty-application-tcp.cc
    model/adaptation-algorithms/fuzzy-algorithm.cc
    model/adaptation-algorithms/fuzzy-algorithm-server.cc
    model/adaptation-algorithms/adaptation-algorithm-server.cc
    model/adaptation-algorithms/google-algorithm-server.cc
    model/adaptation-algorithms/tcp-stream-adaptation-algorithm.cc
    model/adaptation-algorithms/bola.cc
    model/bursty-application-client.cc
    model/bursty-application-server.cc
    model/bursty-application-server-instance.cc
    helper/burst-sink-helper.cc
    helper/bursty-helper.cc
    helper/bursty-application-client-helper.cc
    helper/bursty-application-server-helper.cc
)

set(header_files
    model/burst-generator.h
    model/burst-sink.h
    model/bursty-application.h
    model/my-random-variable-stream.h
    model/seq-ts-size-frag-header.h
    model/simple-burst-generator.h
    model/trace-file-burst-generator.h
    model/vr-adaptive-burst-sink.h
    model/vr-adaptive-bursty-application.h
    model/vr-adaptive-header.h
    model/vr-burst-generator.h
    model/burst-sink-tcp.h
    model/bursty-application-tcp.h
    model/vr-adaptive-burst-sink-tcp.h
    model/vr-adaptive-bursty-application-tcp.h
    model/adaptation-algorithms/fuzzy-algorithm.h
    model/adaptation-algorithms/fuzzy-algorithm-server.h
    model/adaptation-algorithms/adaptation-algorithm-server.h
    model/adaptation-algorithms/google-algorithm-server.h
    model/adaptation-algorithms/tcp-stream-interface.h
    model/adaptation-algorithms/tcp-stream-adaptation-algorithm.h
    model/adaptation-algorithms/bola.h
    model/bursty-application-client.h
    model/bursty-application-server.h
    model/bursty-application-server-instance.h
    helper/burst-sink-helper.h
    helper/bursty-helper.h
    helper/bursty-application-client-helper.h
    helper/bursty-application-server-helper.h
)


set(test_sources

)

build_lib(
  LIBNAME ns-3-adaptive-vr-app
  SOURCE_FILES ${source_files}
  HEADER_FILES ${header_files}
  LIBRARIES_TO_LINK
    ${libcore}
  TEST_SOURCES ${test_sources}
)