set(point_to_point_examples
    bursty-application-example
    vr-application-example
    vr-adaptive-application-example
    trace-file-burst-application-example
)
foreach(
  example
  ${point_to_point_examples}
)
  build_lib_example(
    NAME ${example}
    SOURCE_FILES ${example}.cc
    LIBRARIES_TO_LINK ${libns-3-adaptive-vr-app}
                      ${libapplications}
                      ${libpoint-to-point}
                      ${libinternet}
                      ${libquic}
                      ${libnetwork}
  )
endforeach()


set(wifi_examples
    vr-app-n-stas
    vr-adaptive-app-n-stas
    vr-adaptive-app-n-stas-rev
)
foreach(
  example
  ${wifi_examples}
)
  build_lib_example(
    NAME ${example}
    SOURCE_FILES ${example}.cc
    LIBRARIES_TO_LINK ${libns-3-adaptive-vr-app}
                      ${libcore}
                      ${libapplications}
                      ${libwifi}
                      ${libquic}
  )
endforeach()


set(nr_examples
    test1
    vr-nr-example
    adaptive-cttc-lte-ca-demo
    vr-cttc-fh-compression
)
foreach(
  example
  ${nr_examples}
)
  build_lib_example(
    NAME ${example}
    SOURCE_FILES ${example}.cc
    LIBRARIES_TO_LINK ${libns-3-adaptive-vr-app}
                      ${libcore}
                      ${libapplications}
                      ${libwifi}
                      ${libnr}
                      ${libquic}
                      ${libflow-monitor}
  )
endforeach()