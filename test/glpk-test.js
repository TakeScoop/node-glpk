'use strict;'

const expect = require('code').expect
const testRoot = require('path').resolve(__dirname, '..')
const glp = require('bindings')({ module_root: testRoot, bindings: 'glpk' })
const temp = require('temp').track()
const fs = require('fs')

glp.termOutput(false)

function nearly(n, magnitude) {
    if(!magnitude) {
        magnitude = 1000000000000
    }
    return [ n - 1/magnitude, n + 1/magnitude ]
}

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

describe("Simplex problem tests", function() {
    it('should get the correct answer', function(done) {
        this.timeout(10000)
        lp = setupSimplexLP()
        lp.simplex({}, function() {
            let z = lp.getObjVal()
            let x1 = lp.getColPrim(1)
            let x2 = lp.getColPrim(2)
            let x3 = lp.getColPrim(3)

            expect(lp.getStatus()).to.equal(glp.OPT)
            expect(z).to.be.within(...(nearly(733 + 1/3)))
            expect(x1).to.be.within(...(nearly(33 + 1/3)))
            expect(x2).to.be.within(...(nearly(66 + 2/3)))
            expect(x3).to.equal(0)
            done()
        })
    });
})

describe("Exact problem tests", function() {
    it('should get the correct answer', function(done) {
        this.timeout(10000)
        let lp = setupSimplexLP()
        lp.exact({}, function() {
            let z = lp.getObjVal()
            let x1 = lp.getColPrim(1)
            let x2 = lp.getColPrim(2)
            let x3 = lp.getColPrim(3)

            expect(lp.getStatus()).to.equal(glp.OPT)
            expect(z).to.equal(733 + 1/3)
            expect(x1).to.equal(33 + 1/3)
            expect(x2).to.equal(66 + 2/3)
            expect(x3).to.equal(0)
            done()
        })
    });
})


describe("Intopt problem tests", function() {
    it('should get the correct answer', function(done) {
        this.timeout(10000)
        // LP inspired by sample.c in the glpk distribution
        // modified for MIP; sets 2 columns to integer values
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
        lp.setColBnds(2, glp.LO, 10.0, 0.0);
        lp.setObjCoef(2, 6.0)
        lp.setColKind(2, glp.IV)
        lp.setColName(3, "x2")
        lp.setColBnds(3, glp.LO, 10.0, 0.0);
        lp.setObjCoef(3, 4.0)
        lp.setColKind(3, glp.IV)

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
        lp.simplex({}, function() {
            lp.intopt({},function() {
                let z = lp.mipObjVal()
                let x1 = lp.mipColVal(1)
                let x2 = lp.mipColVal(2)
                let x3 = lp.mipColVal(3)

                expect(lp.mipStatus()).to.equal(glp.OPT)
                expect(z).to.equal(706)
                expect(x1).to.be.within(...(nearly(31.8)))
                expect(x2).to.equal(58)
                expect(x3).to.equal(10)
                done()
            })
        })
    });
})

describe("Interior point problem tests", function() {
    it('should get the correct answer', function(done) {
        this.timeout(10000)
        // LP inspired by sample.c in the glpk distribution
        // Same as simplex LP
        let lp = setupSimplexLP()
        lp.interior({}, function() {
            let z = lp.iptObjVal()
            let x1 = lp.iptColPrim(1)
            let x2 = lp.iptColPrim(2)
            let x3 = lp.iptColPrim(3)

            expect(lp.iptStatus()).to.equal(glp.OPT)
            expect(z).to.be.within(...nearly(733 + 1/3, 10000))
            expect(x1).to.be.within(...(nearly(33 + 1/3, 10000)))
            expect(x2).to.be.within(...(nearly(66 + 2/3, 10000)))
            expect(x3).to.be.within(...(nearly(0, 1000000)))
            done()
        })
    });
})

describe("Factorize problem tests", function() {
    it('should get the correct answer', function(done) {
        this.timeout(10000)
        // LP inspired by sample.c in the glpk distribution
        let lp = setupSimplexLP()
        expect(lp.bfExists()).to.equal(0);
        lp.factorize(function() {
            expect(lp.bfExists()).to.equal(1);
            done()
        })
    });
})

describe("Test glp_intopt_start, glp_intopt_run, glp_intopt_stop flow", function() {
    it("should invoke the callback iteratively toward solution", function(done) {
        this.timeout(10000)
        temp.open('glp_intopt_test_input', function(err, info) {
            fs.write(info.fd,
                'Maximize\n' + 
                'obj: + 786433 x1 + 655361 x2 + 589825 x3 + 557057 x4\n' + 
                '+ 540673 x5 + 532481 x6 + 528385 x7 + 526337 x8 + 525313 x9\n' + 
                '+ 524801 x10 + 524545 x11 + 524417 x12 + 524353 x13\n' + 
                '+ 524321 x14 + 524305 x15\n' + 
                '\n' + 
                'Subject To\n' + 
                'cap: + 786433 x1 + 655361 x2 + 589825 x3 + 557057 x4\n' + 
                '+ 540673 x5 + 532481 x6 + 528385 x7 + 526337 x8 + 525313 x9\n' + 
                '+ 524801 x10 + 524545 x11 + 524417 x12 + 524353 x13\n' + 
                '+ 524321 x14 + 524305 x15 <= 4194303.5\n' + 
                '\n' + 
                'Bounds\n' + 
                '0 <= x1 <= 1\n' + 
                '0 <= x2 <= 1\n' + 
                '0 <= x3 <= 1\n' + 
                '0 <= x4 <= 1\n' + 
                '0 <= x5 <= 1\n' + 
                '0 <= x6 <= 1\n' + 
                '0 <= x7 <= 1\n' + 
                '0 <= x8 <= 1\n' + 
                '0 <= x9 <= 1\n' + 
                '0 <= x10 <= 1\n' + 
                '0 <= x11 <= 1\n' + 
                '0 <= x12 <= 1\n' + 
                '0 <= x13 <= 1\n' + 
                '0 <= x14 <= 1\n' + 
                '0 <= x15 <= 1\n' + 
                '\n' + 
                'Generals\n' + 
                'x1\n' + 
                'x2\n' + 
                'x3\n' + 
                'x4\n' + 
                'x5\n' + 
                'x6\n' + 
                'x7\n' + 
                'x8\n' + 
                'x9\n' + 
                'x10\n' + 
                'x11\n' + 
                'x12\n' + 
                'x13\n' + 
                'x14\n' + 
                'x15\n' + 
                '\n' + 
                'End\n'
            )
            let lp = new glp.Problem();

            lp.readLp(info.path, function(err, ret){
                let calledCallback = 0

                lp.scale(glp.SF_AUTO, function(err){
                    expect(err).to.be.undefined

                    lp.simplex({presolve: glp.ON}, function(err){
                        expect(err).to.be.undefined
                        if (lp.getNumInt() > 0){
                            function callback(tree){
                                calledCallback += 1
                            }

                            lp.intopt({cbFunc: callback}, function(err, ret){
                                expect(err).to.be.null
                                expect(lp.mipObjVal()).to.equal(4190215)
                                // Called 27450 times during, and once after intopt finishes
                                expect(calledCallback).to.equal(27451)
                                done()
                            });
                        }
                    });
                });
            });
        });
    });
})
