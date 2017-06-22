{
	"targets": [
		{
			"target_name": "glpk",
			"sources": [ "src/nodeglpk.cc", "src/problem.hpp", "src/tree.hpp"],
			"cflags": ["-std=c++11", "-Wall", "-Wextra", "-Wno-unused-parameter", "-fexceptions"],
			"cflags_cc": ["-std=c++11", "-Wall", "-Wextra", "-Wno-unused-parameter", "-fexceptions"],
			"defines": [
				"HAVE_ENV"
			],
			"conditions": [
				['OS=="mac"', {
					"xcode_settings": {
						"GCC_ENABLE_CPP_EXCEPTIONS": "YES",
						"CLANG_CXX_LIBRARY": "libc++",
						"GCC_ENABLE_CPP_EXCEPTIONS": "YES",
						"MACOSX_DEPLOYMENT_TARGET": "10.10",
						"OTHER_CPLUSPLUSFLAGS": ["-std=c++11", "-Wall", "-Wextra", "-Wno-unused-parameter", "-fexceptions"],
					}
				}]
			],
			"dependencies": [ "src/glpk/glpk.gyp:libglpk" ],
			"include_dirs": [ "<!(node -e \"require('nan')\")", "<!(node -e 'require(\"cpp-eventemitter\")')" ],
		}
	]
}
