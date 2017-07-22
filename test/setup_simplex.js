'use strict;'
const testRoot = require('path').resolve(__dirname, '..')
const glp = require('bindings')({ module_root: testRoot, bindings: 'glpk' })

glp.termOutput(false)

function setupSimplexLP() {
    // LP inspired by sample.c in the glpk distribution
    let lp = new glp.Problem()
    lp.setProbName("sample")
    lp.setObjDir(glp.MAX)

    lp.addRows(3)
    lp.setRowName(1, "p")
    lp.setRowBnds(1, glp.UP, 0.0, 100.0)
    lp.setRowName(2, "q")
    lp.setRowBnds(2, glp.UP, 0.0, 600.0)
    lp.setRowName(3, "r")
    lp.setRowBnds(3, glp.UP, 0.0, 300.0) 

    lp.addCols(3)
    lp.setColName(1, "x1")
    lp.setColBnds(1, glp.LO, 0.0, 0.0);
    lp.setObjCoef(1, 10.0)
    lp.setColName(2, "x2")
    lp.setColBnds(2, glp.LO, 0.0, 0.0);
    lp.setObjCoef(2, 6.0)
    lp.setColName(3, "x2")
    lp.setColBnds(3, glp.LO, 0.0, 0.0);
    lp.setObjCoef(3, 4.0)

    let ia = new Int32Array(10);
    let ja = new Int32Array(10);
    let ar = new Float64Array(10);

    ia[1] = 1; ja[1] = 1; ar[1] = 1.0;
    ia[2] = 1; ja[2] = 2; ar[2] = 1.0;
    ia[3] = 1; ja[3] = 3; ar[3] = 1.0;
    ia[4] = 2; ja[4] = 1; ar[4] = 10.0;
    ia[5] = 3; ja[5] = 1; ar[5] = 2.0;
    ia[6] = 2; ja[6] = 2; ar[6] = 4.0;
    ia[7] = 3; ja[7] = 2; ar[7] = 2.0;
    ia[8] = 2; ja[8] = 3; ar[8] = 5.0;
    ia[9] = 3; ja[9] = 3; ar[9] = 6.0;

    lp.loadMatrix(9, ia, ja, ar)

    return lp
}

module.exports = {
    setupSimplexLP
}

