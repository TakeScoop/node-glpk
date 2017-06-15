{
  "targets": [
    {
      "target_name": "glpk",
      "sources": [ "src/nodeglpk.cc", "src/problem.hpp", "src/tree.hpp"],
      "cflags": [ "-std=c++14 -fexceptions" ],
      "cflags_cc": [ "-std=c++14 -fexceptions" ],
      "conditions": [
            ['OS=="mac"', {
                "xcode_settings": {
                    "GCC_ENABLE_CPP_EXCEPTIONS": "YES"
                }
            }]
        ],
      "dependencies": [ "src/glpk/glpk.gyp:libglpk" ],
      "include_dirs": [ "<!(node -e \"require('nan')\")" ],
    }
  ]
}
