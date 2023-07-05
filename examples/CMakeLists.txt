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
                      ${libnetwork}
  )
endforeach()


set(wifi_examples
    vr-app-n-stas
    vr-adaptive-app-n-stas
    vr-adaptive-app-n-stas-rev
    vr-a-rev-back
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
  )
endforeach()

