{
  "targets": [
    {
      "target_name": "libeventemitter",
      "type": "static_library",
      "include_dirs": [ "<!(node -e \"require('nan')\")" ],
	  "conditions": [
		['OS=="linux"', {
			"cflags": [ "-Wall", "-Wextra", "-std=c++14", "-fexceptions" ],
			"cflags_cc": [ "-Wall", "-Wextra", "-std=c++14", "-fexceptions" ],
			"ldflags": [ "-pthread" ],
		}],
		['OS=="mac"', {
			"cflags_cc!": [ "-Wall -Wextra -std=c++14 -fexceptions" ],
			"xcode_settings": {
				"GCC_ENABLE_CPP_EXCEPTIONS": "YES",
				"CLANG_CXX_LIBRARY": "libc++",
				"GCC_ENABLE_CPP_EXCEPTIONS": "YES",
				"MACOSX_DEPLOYMENT_TARGET": "10.12",
				"OTHER_CPLUSPLUSFLAGS": [ "-std=c++14", "-Wall", "-Wextra" ],
			}
		}]
	  ],
	  "sources": [
		"eventemitter.hpp",
		"eventemitter.cpp",
		"shared_ringbuffer.hpp",
	  ]
	}
  ]
}
