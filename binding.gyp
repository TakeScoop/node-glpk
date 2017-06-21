{
  "targets": [
    {
      "target_name": "glpk",
      "sources": [ "src/nodeglpk.cc", "src/problem.hpp", "src/tree.hpp", "src/events/eventemitter.hpp", "src/events/shared_ringbuffer.hpp", "src/events/cemitter.h" ],
      "cflags": [ "-std=c++14 -fexceptions" ],
      "cflags_cc!": [ "-Wall -Wextra -std=c++14 -fexceptions" ],
      "conditions": [
            ['OS=="mac"', {
				"xcode_settings": {
					"GCC_ENABLE_CPP_EXCEPTIONS": "YES",
					"CLANG_CXX_LIBRARY": "libc++",
					"GCC_ENABLE_CPP_EXCEPTIONS": "YES",
					"MACOSX_DEPLOYMENT_TARGET": "10.12",
					"OTHER_CPLUSPLUSFLAGS": [ "-std=c++14", "-Wall", "-Wextra", "-fexceptions" ],
				}
            }]
        ],
      "dependencies": [ "src/glpk/glpk.gyp:libglpk" ],
      "include_dirs": [ "<!(node -e \"require('nan')\")" ],
    }
  ]
}
