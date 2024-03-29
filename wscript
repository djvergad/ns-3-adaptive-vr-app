# -*- Mode: python; py-indent-offset: 4; indent-tabs-mode: nil; coding: utf-8; -*-

def build(bld):
    module = bld.create_ns3_module('vr-app', ['core'])
    module.source = [
        'model/burst-generator.cc',
        'model/burst-sink.cc',
        'model/bursty-application.cc',
        'model/my-random-variable-stream.cc',
        'model/seq-ts-size-frag-header.cc',
        'model/simple-burst-generator.cc',
        'model/trace-file-burst-generator.cc',
        'model/vr-adaptive-burst-sink.cc',
        'model/vr-adaptive-bursty-application.cc',
        'model/vr-adaptive-header.cc',
        'model/vr-burst-generator.cc',
        'model/burst-sink-tcp.cc',
        'model/bursty-application-tcp.cc',
        'model/vr-adaptive-burst-sink-tcp.cc',
        'model/vr-adaptive-bursty-application-tcp.cc',
        'model/adaptation-algorithms/fuzzy-algorithm.cc',
        'model/adaptation-algorithms/fuzzy-algorithm-server.cc',
        'model/adaptation-algorithms/adaptation-algorithm-server.cc',
        'model/adaptation-algorithms/google-algorithm-server.cc',
        'model/bursty-application-client.cc',
        'model/bursty-application-server.cc',
        'model/bursty-application-server-instance.cc',
        'helper/burst-sink-helper.cc',
        'helper/bursty-helper.cc',
        'helper/bursty-application-client-helper.cc',
        'helper/bursty-application-server-helper.cc',
    ]

    headers = bld(features='ns3header')
    headers.module = 'vr-app'
    headers.source = [
        'model/burst-generator.h',
        'model/burst-sink.h',
        'model/bursty-application.h',
        'model/my-random-variable-stream.h',
        'model/seq-ts-size-frag-header.h',
        'model/simple-burst-generator.h',
        'model/trace-file-burst-generator.h',
        'model/vr-adaptive-burst-sink.h',
        'model/vr-adaptive-bursty-application.h',
        'model/vr-adaptive-header.h',
        'model/vr-burst-generator.h',
        'model/burst-sink-tcp.h',
        'model/bursty-application-tcp.h',
        'model/vr-adaptive-burst-sink-tcp.h',
        'model/vr-adaptive-bursty-application-tcp.h',
        'model/adaptation-algorithms/fuzzy-algorithm.h',
        'model/adaptation-algorithms/fuzzy-algorithm-server.h',
        'model/adaptation-algorithms/adaptation-algorithm-server.h',
        'model/adaptation-algorithms/google-algorithm-server.h',
        'model/bursty-application-client.h',
        'model/bursty-application-server.h',
        'model/bursty-application-server-instance.h',
        'helper/burst-sink-helper.h',
        'helper/bursty-helper.h',
        'helper/bursty-application-client-helper.h',
        'helper/bursty-application-server-helper.h',
    ]

    if (bld.env['ENABLE_EXAMPLES']):
        bld.recurse('examples')

    bld.ns3_python_bindings()
