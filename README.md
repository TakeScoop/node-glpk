[![Build Status](https://travis-ci.org/hgourvest/node-glpk.svg?branch=master)](https://travis-ci.org/hgourvest/node-glpk)

# DEPRECATED :no_entry:
This repo is not maintained anymore. Please see [glpk.js](https://github.com/jvail/glpk.js) for an alternative.

# node-glpk
Node.js native module for GLPK.

## Install
```sh
$ nvm use
$ npm install
```

## Test
```sh
$ npm run test
```

## Example
```js
var glp = require("glpk");
var prob = new glp.Problem();
 
prob.readLpSync("todd.lpt");
prob.scaleSync(glp.SF_AUTO);
prob.simplexSync({presolve: glp.ON});
if (prob.getNumInt() > 0){
  function callback(tree){
    if (tree.reason() == glp.IBINGO){
      // ...
    }
  }
  prob.intoptSync({cbFunc: callback});
}
console.log("objective: " + prob.mipObjVal());
prob.delete();
```
